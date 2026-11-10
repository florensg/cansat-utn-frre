#include <Arduino.h>
#include <Wire.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "flight_core/commands.h"
#include "flight_core/flight.h"
#include "flight_core/fsm.h"
#include "flight_core/sensors.h"
#include "flight_core/storage.h"
#include "flight_core/telemetry.h"

namespace {

using cansat::ApogeeDetector;
using cansat::CommandType;
using cansat::Event;
using cansat::FlightEvent;
using cansat::FlightStateMachine;
using cansat::MemoryStorage;
using cansat::PersistentState;
using cansat::SensorSample;
using cansat::State;
using cansat::TelemetryFrame;

FlightStateMachine g_fsm;
ApogeeDetector g_apogee({cansat::kMinApogeeAltitudeM, cansat::kApogeeDropM,
                         cansat::kSecondParachuteRatio,
                         cansat::kMinFlightTimeMs});
MemoryStorage g_ram_storage;
TelemetryFrame g_frame{};
PersistentState g_persistent{};

uint32_t g_packet_count = 0;
uint32_t g_mission_time_offset_ms = 0;
uint32_t g_last_telemetry_ms = 0;
uint32_t g_last_servo_pulse_ms = 0;
uint32_t g_launch_start_ms = 0;

bool g_mpu_available = false;
bool g_baro_available = false;
bool g_ina_available = false;
bool g_fram_available = false;
bool g_lis_available = false;
bool g_ads_available = false;
bool g_xiao_vis_available = false;
bool g_xiao_nir_available = false;

bool g_mechanism_activated = false;
char g_last_cmd_echo[16] = "NONE";
char g_rpi_payload_summary[64] = "";
char g_optional_buffer[160] = "";

// Tiempo UTC / mision derivado de GPS ($GPGGA / $GNGGA) o comando ST + millis()
struct TimeKeeper {
  bool synced = false;
  uint32_t base_seconds_of_day = 0;
  uint32_t base_millis = 0;

  void sync_hms(uint8_t h, uint8_t m, uint8_t s, uint32_t now_ms) {
    if (h < 24 && m < 60 && s < 60) {
      base_seconds_of_day =
          static_cast<uint32_t>(h) * 3600u + static_cast<uint32_t>(m) * 60u + s;
      base_millis = now_ms;
      synced = true;
    }
  }

  void get_hms(uint32_t now_ms, uint8_t& h, uint8_t& m, uint8_t& s,
               uint8_t& valid) const {
    const uint32_t elapsed_s = (now_ms - base_millis) / 1000u;
    const uint32_t total_s = (base_seconds_of_day + elapsed_s) % 86400u;
    h = static_cast<uint8_t>(total_s / 3600u);
    m = static_cast<uint8_t>((total_s % 3600u) / 60u);
    s = static_cast<uint8_t>(total_s % 60u);
    valid = synced ? 1u : 0u;
  }
};

TimeKeeper g_clock;

// Datos reportados por la Raspberry Pi Zero W (LIS3MDL + ADS1115/TSL251RD + actitud)
struct RpiPayloadState {
  bool valid = false;
  uint32_t last_rx_ms = 0;
  float mag_x_ut = 0.0f;
  float mag_y_ut = 0.0f;
  float mag_z_ut = 0.0f;
  uint16_t optical_raw[4] = {0, 0, 0, 0};
  float roll_deg = 0.0f;
  float pitch_deg = 0.0f;
  float yaw_deg = 0.0f;
  char line[160] = "";
  uint8_t line_len = 0;
};

RpiPayloadState g_rpi;

// ---------------------------------------------------------------------------
// Utilidades I2C (soporta bus principal Wire y bus alternativo Wire1 si IEM 3
// separa I2C en GPIO 13/14 mientras BMP280 usa SPI en GPIO 8/9)
// ---------------------------------------------------------------------------
TwoWire* g_i2c_bus = &Wire;

bool i2c_probe_on(TwoWire& bus, uint8_t address) {
  bus.beginTransmission(address);
  return bus.endTransmission() == 0;
}

bool i2c_read_on(TwoWire& bus, uint8_t address, uint8_t reg, uint8_t* buffer,
                 uint8_t length) {
  bus.beginTransmission(address);
  bus.write(reg);
  if (bus.endTransmission(false) != 0) {
    return false;
  }
  if (bus.requestFrom(address, length) != length) {
    return false;
  }
  for (uint8_t i = 0; i < length; ++i) {
    buffer[i] = static_cast<uint8_t>(bus.read());
  }
  return true;
}

bool i2c_read(uint8_t address, uint8_t reg, uint8_t* buffer, uint8_t length) {
  return i2c_read_on(*g_i2c_bus, address, reg, buffer, length);
}

bool i2c_write_u8(uint8_t address, uint8_t reg, uint8_t value) {
  g_i2c_bus->beginTransmission(address);
  g_i2c_bus->write(reg);
  g_i2c_bus->write(value);
  return g_i2c_bus->endTransmission() == 0;
}

bool i2c_write_u16(uint8_t address, uint8_t reg, uint16_t value) {
  g_i2c_bus->beginTransmission(address);
  g_i2c_bus->write(reg);
  g_i2c_bus->write(static_cast<uint8_t>(value >> 8));
  g_i2c_bus->write(static_cast<uint8_t>(value & 0xFF));
  return g_i2c_bus->endTransmission() == 0;
}

uint16_t le_u16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
int16_t le_s16(const uint8_t* p) { return static_cast<int16_t>(le_u16(p)); }
uint16_t be_u16(const uint8_t* p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}
int16_t be_s16(const uint8_t* p) { return static_cast<int16_t>(be_u16(p)); }

// ---------------------------------------------------------------------------
// VRAM1: FRAM MB85RC256V (I2C 0x50, direccionamiento de 16 bits, 32 KB)
// Guarda PersistentState con doble copia (offset 0x0000 y 0x0040) + checksum.
// ---------------------------------------------------------------------------
bool fram_write_bytes(uint16_t mem_addr, const uint8_t* data, std::size_t len) {
  if (!g_fram_available) {
    return false;
  }
  g_i2c_bus->beginTransmission(cansat::kFramI2cAddress);
  g_i2c_bus->write(static_cast<uint8_t>((mem_addr >> 8) & 0x7F));
  g_i2c_bus->write(static_cast<uint8_t>(mem_addr & 0xFF));
  for (std::size_t i = 0; i < len; ++i) {
    g_i2c_bus->write(data[i]);
  }
  return g_i2c_bus->endTransmission() == 0;
}

bool fram_read_bytes(uint16_t mem_addr, uint8_t* data, std::size_t len) {
  if (!g_fram_available) {
    return false;
  }
  g_i2c_bus->beginTransmission(cansat::kFramI2cAddress);
  g_i2c_bus->write(static_cast<uint8_t>((mem_addr >> 8) & 0x7F));
  g_i2c_bus->write(static_cast<uint8_t>(mem_addr & 0xFF));
  if (g_i2c_bus->endTransmission(false) != 0) {
    return false;
  }
  const std::size_t got = g_i2c_bus->requestFrom(
      cansat::kFramI2cAddress, static_cast<uint8_t>(len));
  if (got != len) {
    return false;
  }
  for (std::size_t i = 0; i < len; ++i) {
    data[i] = static_cast<uint8_t>(g_i2c_bus->read());
  }
  return true;
}

bool init_fram() {
  return i2c_probe_on(*g_i2c_bus, cansat::kFramI2cAddress);
}

bool storage_load_state(PersistentState& out) {
  if (g_fram_available) {
    PersistentState copy_a{};
    if (fram_read_bytes(0x0000, reinterpret_cast<uint8_t*>(&copy_a),
                        sizeof(copy_a)) &&
        copy_a.magic == cansat::kPersistentMagic &&
        copy_a.version == cansat::kPersistentVersion &&
        copy_a.checksum == cansat::compute_checksum(copy_a)) {
      out = copy_a;
      return true;
    }
    PersistentState copy_b{};
    if (fram_read_bytes(0x0040, reinterpret_cast<uint8_t*>(&copy_b),
                        sizeof(copy_b)) &&
        copy_b.magic == cansat::kPersistentMagic &&
        copy_b.version == cansat::kPersistentVersion &&
        copy_b.checksum == cansat::compute_checksum(copy_b)) {
      out = copy_b;
      return true;
    }
  }
  return g_ram_storage.load(out);
}

void storage_save_state(const PersistentState& state) {
  g_ram_storage.save(state);
  if (g_fram_available) {
    PersistentState copy = state;
    copy.checksum = cansat::compute_checksum(copy);
    fram_write_bytes(0x0000, reinterpret_cast<const uint8_t*>(&copy),
                     sizeof(copy));
    fram_write_bytes(0x0040, reinterpret_cast<const uint8_t*>(&copy),
                     sizeof(copy));
  }
}

// ---------------------------------------------------------------------------
// MPU1: IMU MPU-6050 (I2C 0x68, con pin INT en GPIO 14)
// ---------------------------------------------------------------------------
bool init_mpu6050() {
  if (!i2c_write_u8(cansat::kMpu6050I2cAddress, 0x6B, 0x00)) {  // PWR_MGMT_1
    return false;
  }
  // Habilita DATA_RDY_EN en INT_ENABLE (0x38) para el pin INT conectado a GPIO14
  i2c_write_u8(cansat::kMpu6050I2cAddress, 0x38, 0x01);
  return true;
}

bool read_mpu6050(SensorSample& sample) {
  if (!g_mpu_available) {
    return false;
  }
  uint8_t raw[14];
  if (!i2c_read(cansat::kMpu6050I2cAddress, 0x3B, raw, sizeof(raw))) {
    return false;
  }

  const int16_t ax = be_s16(raw + 0);
  const int16_t ay = be_s16(raw + 2);
  const int16_t az = be_s16(raw + 4);
  const int16_t temp_raw = be_s16(raw + 6);
  const int16_t gx = be_s16(raw + 8);
  const int16_t gy = be_s16(raw + 10);
  const int16_t gz = be_s16(raw + 12);

  sample.accel_x_g = static_cast<float>(ax) / 16384.0f;
  sample.accel_y_g = static_cast<float>(ay) / 16384.0f;
  sample.accel_z_g = static_cast<float>(az) / 16384.0f;
  sample.temperature_c = (static_cast<float>(temp_raw) / 340.0f) + 36.53f;
  sample.gyro_x_dps = static_cast<float>(gx) / 131.0f;
  sample.gyro_y_dps = static_cast<float>(gy) / 131.0f;
  sample.gyro_z_dps = static_cast<float>(gz) / 131.0f;
  return true;
}

// ---------------------------------------------------------------------------
// U2: Barometro BMP280 (soporta I2C 0x76/0x77 y SPI en GPIO 8/9/4/5 segun v3)
// ---------------------------------------------------------------------------
enum class BaroTransport : uint8_t { None = 0, I2c = 1, Spi = 2 };

struct Barometer {
  uint8_t chip_id = 0;  // 0x58 = BMP280
  uint8_t address = 0;
  BaroTransport transport = BaroTransport::None;

  uint16_t dig_T1 = 0;
  int16_t dig_T2 = 0;
  int16_t dig_T3 = 0;
  uint16_t dig_P1 = 0;
  int16_t dig_P2 = 0;
  int16_t dig_P3 = 0;
  int16_t dig_P4 = 0;
  int16_t dig_P5 = 0;
  int16_t dig_P6 = 0;
  int16_t dig_P7 = 0;
  int16_t dig_P8 = 0;
  int16_t dig_P9 = 0;
  int32_t t_fine = 0;
};

Barometer g_baro;
float g_baro_baseline_pa = 101325.0f;
float g_baro_altitude_m = 0.0f;
float g_baro_last_pa = 0.0f;

// Transferencia SPI por software (Mode 0) en los pines del esquema v3
uint8_t bmp280_spi_transfer_byte(uint8_t out_byte) {
  uint8_t in_byte = 0;
  for (int bit = 7; bit >= 0; --bit) {
    digitalWrite(cansat::kBmp280SpiMosiPin,
                 ((out_byte >> bit) & 0x01) ? HIGH : LOW);
    delayMicroseconds(1);
    digitalWrite(cansat::kBmp280SpiSckPin, HIGH);
    delayMicroseconds(1);
    in_byte = static_cast<uint8_t>(
        (in_byte << 1) | (digitalRead(cansat::kBmp280SpiMisoPin) ? 1 : 0));
    digitalWrite(cansat::kBmp280SpiSckPin, LOW);
  }
  return in_byte;
}

bool bmp280_read_regs(uint8_t reg, uint8_t* buffer, uint8_t length) {
  if (g_baro.transport == BaroTransport::Spi) {
    digitalWrite(cansat::kBmp280SpiCsPin, LOW);
    bmp280_spi_transfer_byte(reg | 0x80);
    for (uint8_t i = 0; i < length; ++i) {
      buffer[i] = bmp280_spi_transfer_byte(0x00);
    }
    digitalWrite(cansat::kBmp280SpiCsPin, HIGH);
    return true;
  }
  return i2c_read(g_baro.address, reg, buffer, length);
}

bool bmp280_write_reg(uint8_t reg, uint8_t value) {
  if (g_baro.transport == BaroTransport::Spi) {
    digitalWrite(cansat::kBmp280SpiCsPin, LOW);
    bmp280_spi_transfer_byte(reg & 0x7F);
    bmp280_spi_transfer_byte(value);
    digitalWrite(cansat::kBmp280SpiCsPin, HIGH);
    return true;
  }
  return i2c_write_u8(g_baro.address, reg, value);
}

bool init_bmp280_common() {
  uint8_t cal[24];
  if (!bmp280_read_regs(0x88, cal, sizeof(cal))) {
    return false;
  }
  g_baro.dig_T1 = le_u16(cal + 0);
  g_baro.dig_T2 = le_s16(cal + 2);
  g_baro.dig_T3 = le_s16(cal + 4);
  g_baro.dig_P1 = le_u16(cal + 6);
  g_baro.dig_P2 = le_s16(cal + 8);
  g_baro.dig_P3 = le_s16(cal + 10);
  g_baro.dig_P4 = le_s16(cal + 12);
  g_baro.dig_P5 = le_s16(cal + 14);
  g_baro.dig_P6 = le_s16(cal + 16);
  g_baro.dig_P7 = le_s16(cal + 18);
  g_baro.dig_P8 = le_s16(cal + 20);
  g_baro.dig_P9 = le_s16(cal + 22);

  if (!bmp280_write_reg(0xF4, 0x27)) {  // temp x2, press x16, modo normal
    return false;
  }
  g_baro.chip_id = 0x58;
  return true;
}

bool init_barometer() {
  // 1) Intentar BMP280 por I2C (0x76 / 0x77)
  const uint8_t candidates[2] = {cansat::kBmp280I2cAddress,
                                 cansat::kBmp280AltI2cAddress};
  for (uint8_t i = 0; i < 2; ++i) {
    uint8_t id = 0;
    if (!i2c_read(candidates[i], 0xD0, &id, 1)) {
      continue;
    }
    if (id == 0x58) {
      g_baro.address = candidates[i];
      g_baro.transport = BaroTransport::I2c;
      if (init_bmp280_common()) {
        Serial.printf("BARO: BMP280 I2C detectado en 0x%02X\n", candidates[i]);
        return true;
      }
    }
  }

  // 2) Si no esta en I2C, probar BMP280 por SPI segun esquema v3
  //    (CSB=GPIO4, SDI=GPIO8, SCK=GPIO9, SDO=GPIO5) cuando el bus I2C usa Wire1
  if (g_i2c_bus == &Wire1) {
    pinMode(cansat::kBmp280SpiCsPin, OUTPUT);
    digitalWrite(cansat::kBmp280SpiCsPin, HIGH);
    pinMode(cansat::kBmp280SpiMosiPin, OUTPUT);
    pinMode(cansat::kBmp280SpiSckPin, OUTPUT);
    digitalWrite(cansat::kBmp280SpiSckPin, LOW);
    pinMode(cansat::kBmp280SpiMisoPin, INPUT_PULLUP);

    g_baro.transport = BaroTransport::Spi;
    uint8_t spi_id = 0;
    if (bmp280_read_regs(0xD0, &spi_id, 1) && spi_id == 0x58 &&
        init_bmp280_common()) {
      Serial.println("BARO: BMP280 SPI detectado (CS=GPIO4)");
      return true;
    }
    g_baro.transport = BaroTransport::None;
  }

  Serial.println("BARO: FAIL (no detectado)");
  return false;
}

float bmp280_temperature_c() {
  uint8_t raw[3];
  if (!bmp280_read_regs(0xFA, raw, 3)) {
    return NAN;
  }
  const int32_t adc_T = static_cast<int32_t>(
      (static_cast<uint32_t>(raw[0]) << 12) |
      (static_cast<uint32_t>(raw[1]) << 4) | (raw[2] >> 4));
  int32_t var1 =
      ((((adc_T >> 3) - (static_cast<int32_t>(g_baro.dig_T1) << 1))) *
       g_baro.dig_T2) >>
      11;
  int32_t var2 =
      (((((adc_T >> 4) - static_cast<int32_t>(g_baro.dig_T1)) *
         ((adc_T >> 4) - static_cast<int32_t>(g_baro.dig_T1))) >>
        12) *
       g_baro.dig_T3) >>
      14;
  g_baro.t_fine = var1 + var2;
  return static_cast<float>((g_baro.t_fine * 5 + 128) >> 8) / 100.0f;
}

float bmp280_pressure_pa() {
  uint8_t raw[3];
  if (!bmp280_read_regs(0xF7, raw, 3)) {
    return NAN;
  }
  const int32_t adc_P = static_cast<int32_t>(
      (static_cast<uint32_t>(raw[0]) << 12) |
      (static_cast<uint32_t>(raw[1]) << 4) | (raw[2] >> 4));

  int64_t var1 = static_cast<int64_t>(g_baro.t_fine) - 128000;
  int64_t var2 = var1 * var1 * static_cast<int64_t>(g_baro.dig_P6);
  var2 += (var1 * static_cast<int64_t>(g_baro.dig_P5)) << 17;
  var2 += static_cast<int64_t>(g_baro.dig_P4) << 35;
  var1 = ((var1 * var1 * static_cast<int64_t>(g_baro.dig_P3)) >> 8) +
         ((var1 * static_cast<int64_t>(g_baro.dig_P2)) << 12);
  var1 = ((static_cast<int64_t>(1) << 47) + var1) *
         static_cast<int64_t>(g_baro.dig_P1) >> 33;
  if (var1 == 0) {
    return NAN;
  }
  int64_t p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (static_cast<int64_t>(g_baro.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (static_cast<int64_t>(g_baro.dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (static_cast<int64_t>(g_baro.dig_P7) << 4);
  return static_cast<float>(p) / 256.0f;
}

bool read_barometer(float& pressure_pa, float& temperature_c,
                    float& altitude_m) {
  if (g_baro.chip_id != 0x58) {
    return false;
  }
  const float t = bmp280_temperature_c();
  if (std::isnan(t)) {
    return false;
  }
  const float p = bmp280_pressure_pa();
  if (std::isnan(p) || p <= 0.0f) {
    return false;
  }
  temperature_c = t;
  pressure_pa = p;

  g_baro_last_pa = pressure_pa;
  const float ratio = pressure_pa / g_baro_baseline_pa;
  altitude_m = 44330.0f * (1.0f - std::pow(ratio, 1.0f / 5.255f));
  return true;
}

// ---------------------------------------------------------------------------
// INA1: INA219 (I2C 0x40): tension de bus y corriente de bateria (BAT1 3.7 V)
// ---------------------------------------------------------------------------
bool init_ina219() {
  return i2c_write_u16(cansat::kIna219I2cAddress, 0x00, 0x399F);
}

bool read_ina219(float& battery_v, float& current_ma) {
  uint8_t raw[2];
  if (!i2c_read(cansat::kIna219I2cAddress, 0x02, raw, 2)) {
    return false;
  }
  const uint16_t bus_raw = be_u16(raw);
  battery_v = static_cast<float>(bus_raw >> 3) * 0.004f;

  if (!i2c_read(cansat::kIna219I2cAddress, 0x01, raw, 2)) {
    return false;
  }
  const int16_t shunt_raw = be_s16(raw);
  current_ma = static_cast<float>(shunt_raw) * 10.0f /
               cansat::kIna219ShuntOhms / 1000.0f;
  return true;
}

// ---------------------------------------------------------------------------
// LIS3MDL (I2C 0x1C) y ADS1115 (I2C 0x48):
// En el esquema v3 estan conectados al bus I2C de la Raspberry Pi Zero W, que
// envia sus lecturas al OBC por UART. Tambien se soporta lectura directa por I2C.
// ---------------------------------------------------------------------------
bool init_lis3mdl() {
  uint8_t who = 0;
  if (!i2c_read(cansat::kLis3mdlI2cAddress, 0x0F, &who, 1) || who != 0x3D) {
    return false;
  }
  return i2c_write_u8(cansat::kLis3mdlI2cAddress, 0x20, 0x7C) &&
         i2c_write_u8(cansat::kLis3mdlI2cAddress, 0x21, 0x00) &&
         i2c_write_u8(cansat::kLis3mdlI2cAddress, 0x22, 0x00) &&
         i2c_write_u8(cansat::kLis3mdlI2cAddress, 0x23, 0x0C);
}

bool read_lis3mdl(float& mx_ut, float& my_ut, float& mz_ut) {
  if (!g_lis_available) {
    return false;
  }
  uint8_t raw[6];
  if (!i2c_read(cansat::kLis3mdlI2cAddress, 0x28 | 0x80, raw, 6)) {
    return false;
  }
  constexpr float kLsbPerUt = 68.42f;
  mx_ut = static_cast<float>(le_s16(raw + 0)) / kLsbPerUt;
  my_ut = static_cast<float>(le_s16(raw + 2)) / kLsbPerUt;
  mz_ut = static_cast<float>(le_s16(raw + 4)) / kLsbPerUt;
  return true;
}

bool init_ads1115() {
  return i2c_write_u16(cansat::kAds1115I2cAddress, 0x01, 0xC383);
}

bool ads1115_read_channel(uint8_t mux, uint16_t& raw_value) {
  const uint16_t config =
      static_cast<uint16_t>(0xC383 | ((mux & 0x07) << 12));
  if (!i2c_write_u16(cansat::kAds1115I2cAddress, 0x01, config)) {
    return false;
  }
  delay(9);
  uint8_t raw[2];
  if (!i2c_read(cansat::kAds1115I2cAddress, 0x00, raw, 2)) {
    return false;
  }
  raw_value = be_u16(raw);
  return true;
}

void read_magnetometer_and_optical(SensorSample& sample, uint32_t now_ms) {
  // Prioridad 1: Lectura recibida de la Raspberry Pi Zero W por UART (< 3 s)
  if (g_rpi.valid && (now_ms - g_rpi.last_rx_ms) <= 3000u) {
    sample.mag_x_ut = g_rpi.mag_x_ut;
    sample.mag_y_ut = g_rpi.mag_y_ut;
    sample.mag_z_ut = g_rpi.mag_z_ut;
    for (uint8_t i = 0; i < 4; ++i) {
      sample.optical_raw[i] = g_rpi.optical_raw[i];
    }
    return;
  }

  // Prioridad 2: Si LIS3MDL / ADS1115 estan conectados directo al bus I2C del OBC
  if (g_lis_available) {
    if (!read_lis3mdl(sample.mag_x_ut, sample.mag_y_ut, sample.mag_z_ut)) {
      g_lis_available = false;
    }
  } else {
    sample.mag_x_ut = 0.0f;
    sample.mag_y_ut = 0.0f;
    sample.mag_z_ut = 0.0f;
  }

  if (g_ads_available) {
    bool ok = true;
    for (uint8_t i = 0; i < 4; ++i) {
      ok = ads1115_read_channel(static_cast<uint8_t>(4 + i),
                                sample.optical_raw[i]) &&
           ok;
    }
    if (ok) {
      return;
    }
    g_ads_available = false;
  }

  for (uint8_t i = 0; i < 4; ++i) {
    sample.optical_raw[i] = 0;
  }
}

// ---------------------------------------------------------------------------
// U8 / U9: Camaras XIAO ESP32-S3 Sense conectadas por I2C (D4=SDA, D5=SCL)
// Permiten disparo sincronizado de capturas VIS (0x30) y NIR (0x31).
// ---------------------------------------------------------------------------
uint16_t g_xiao_vis_captures = 0;
uint16_t g_xiao_nir_captures = 0;

void init_xiao_cameras() {
  g_xiao_vis_available = i2c_probe_on(*g_i2c_bus, cansat::kXiaoVisI2cAddress);
  g_xiao_nir_available = i2c_probe_on(*g_i2c_bus, cansat::kXiaoNirI2cAddress);
}

void trigger_xiao_capture(uint8_t command_byte) {
  if (g_xiao_vis_available) {
    i2c_write_u8(cansat::kXiaoVisI2cAddress, 0x01, command_byte);
  }
  if (g_xiao_nir_available) {
    i2c_write_u8(cansat::kXiaoNirI2cAddress, 0x01, command_byte);
  }
}

void poll_xiao_status() {
  uint8_t buf[3];
  if (g_xiao_vis_available &&
      i2c_read(cansat::kXiaoVisI2cAddress, 0x02, buf, 3)) {
    g_xiao_vis_captures = be_u16(buf + 1);
  }
  if (g_xiao_nir_available &&
      i2c_read(cansat::kXiaoNirI2cAddress, 0x02, buf, 3)) {
    g_xiao_nir_captures = be_u16(buf + 1);
  }
}

// ---------------------------------------------------------------------------
// U3: GPS NEO-M8N (UART1 @ 9600). Extrae posicion, sat, fix y hora UTC.
// ---------------------------------------------------------------------------
struct GpsState {
  bool fix = false;
  uint8_t satellites = 0;
  float latitude_deg = 0.0f;
  float longitude_deg = 0.0f;
  float altitude_m = 0.0f;
  char line[96] = "";
  uint8_t line_len = 0;
};

GpsState g_gps;

float nmea_to_degrees(const char* value, char hemisphere) {
  if (value == nullptr || value[0] == '\0') {
    return 0.0f;
  }
  const float raw = static_cast<float>(std::atof(value));
  const int degrees = static_cast<int>(raw / 100.0f);
  const float minutes = raw - static_cast<float>(degrees) * 100.0f;
  float result = static_cast<float>(degrees) + minutes / 60.0f;
  if (hemisphere == 'S' || hemisphere == 'W') {
    result = -result;
  }
  return result;
}

void parse_nmea_line(char* line, uint32_t now_ms) {
  if (std::strncmp(line, "$GPGGA", 6) != 0 &&
      std::strncmp(line, "$GNGGA", 6) != 0) {
    return;
  }
  char* field[15] = {nullptr};
  uint8_t count = 0;
  field[count++] = line;
  for (char* p = line; *p != '\0' && count < 15; ++p) {
    if (*p == ',') {
      *p = '\0';
      field[count++] = p + 1;
    }
  }
  if (count < 10) {
    return;
  }
  // Campo 1: hhmmss.ss (hora UTC del GPS)
  if (field[1] != nullptr && std::strlen(field[1]) >= 6) {
    const int hhmmss = std::atoi(field[1]);
    const uint8_t hh = static_cast<uint8_t>((hhmmss / 10000) % 24);
    const uint8_t mm = static_cast<uint8_t>((hhmmss / 100) % 100);
    const uint8_t ss = static_cast<uint8_t>(hhmmss % 100);
    g_clock.sync_hms(hh, mm, ss, now_ms);
  }
  const int fix_quality = std::atoi(field[6]);
  g_gps.fix = fix_quality > 0;
  g_gps.satellites = static_cast<uint8_t>(std::atoi(field[7]) & 0xFF);
  if (!g_gps.fix) {
    return;
  }
  g_gps.latitude_deg = nmea_to_degrees(field[2], field[3][0]);
  g_gps.longitude_deg = nmea_to_degrees(field[4], field[5][0]);
  g_gps.altitude_m = static_cast<float>(std::atof(field[9]));
}

void poll_gps(uint32_t now_ms) {
  while (Serial1.available() > 0) {
    const char c = static_cast<char>(Serial1.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      g_gps.line[g_gps.line_len] = '\0';
      if (g_gps.line_len > 6) {
        parse_nmea_line(g_gps.line, now_ms);
      }
      g_gps.line_len = 0;
      continue;
    }
    if (g_gps.line_len < sizeof(g_gps.line) - 1) {
      g_gps.line[g_gps.line_len++] = c;
    } else {
      g_gps.line_len = 0;
    }
  }
}

// ---------------------------------------------------------------------------
// Enlace UART2 con XBee S1 Module y Raspberry Pi Zero W (GPIO14/15 en RPi).
// La RPi envia tramas con el formato:
//   $RPI,mx,my,mz,opt1,opt2,opt3,opt4,roll,pitch,yaw,tag
// y tambien se aceptan telecomandos desde el XBee por el mismo puerto.
// ---------------------------------------------------------------------------
void handle_command(const char* line, uint32_t now_ms);

void parse_rpi_line(char* line, uint32_t now_ms) {
  if (std::strncmp(line, "$RPI,", 5) != 0) {
    handle_command(line, now_ms);
    return;
  }
  char* field[16] = {nullptr};
  uint8_t count = 0;
  field[count++] = line;
  for (char* p = line; *p != '\0' && count < 16; ++p) {
    if (*p == ',') {
      *p = '\0';
      field[count++] = p + 1;
    }
  }
  if (count < 8) {
    return;
  }
  g_rpi.mag_x_ut = static_cast<float>(std::atof(field[1]));
  g_rpi.mag_y_ut = static_cast<float>(std::atof(field[2]));
  g_rpi.mag_z_ut = static_cast<float>(std::atof(field[3]));
  g_rpi.optical_raw[0] = static_cast<uint16_t>(std::atoi(field[4]) & 0xFFFF);
  g_rpi.optical_raw[1] = static_cast<uint16_t>(std::atoi(field[5]) & 0xFFFF);
  g_rpi.optical_raw[2] = static_cast<uint16_t>(std::atoi(field[6]) & 0xFFFF);
  g_rpi.optical_raw[3] = static_cast<uint16_t>(std::atoi(field[7]) & 0xFFFF);
  if (count >= 11) {
    g_rpi.roll_deg = static_cast<float>(std::atof(field[8]));
    g_rpi.pitch_deg = static_cast<float>(std::atof(field[9]));
    g_rpi.yaw_deg = static_cast<float>(std::atof(field[10]));
  }
  if (count >= 12 && field[11] != nullptr) {
    std::snprintf(g_rpi_payload_summary, sizeof(g_rpi_payload_summary), "%s",
                  field[11]);
  }
  g_rpi.valid = true;
  g_rpi.last_rx_ms = now_ms;
}

void poll_radio_and_rpi(uint32_t now_ms) {
  while (Serial2.available() > 0) {
    const char c = static_cast<char>(Serial2.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      g_rpi.line[g_rpi.line_len] = '\0';
      if (g_rpi.line_len > 1) {
        parse_rpi_line(g_rpi.line, now_ms);
      }
      g_rpi.line_len = 0;
      continue;
    }
    if (g_rpi.line_len < sizeof(g_rpi.line) - 1) {
      g_rpi.line[g_rpi.line_len++] = c;
    } else {
      g_rpi.line_len = 0;
    }
  }
}

void pulse_servo(uint32_t now_ms) {
  if ((now_ms - g_last_servo_pulse_ms) < 20) {
    return;
  }
  g_last_servo_pulse_ms = now_ms;
  const bool para2_open =
      g_apogee.second_parachute_triggered() || g_mechanism_activated;
  const int angle_deg = para2_open ? 90 : 0;
  const int pulse_us = 544 + (angle_deg * (2400 - 544)) / 180;

  digitalWrite(cansat::kPinServoV3, HIGH);
  delayMicroseconds(pulse_us);
  digitalWrite(cansat::kPinServoV3, LOW);
}

void apply_sample(const SensorSample& sample) {
  g_frame.altitude_m = sample.altitude_m - g_persistent.altitude_offset_m;
  g_frame.pressure_pa = sample.pressure_pa;
  g_frame.temperature_c = sample.temperature_c;
  g_frame.battery_v = sample.battery_v;
  g_frame.current_ma = sample.current_ma;
  g_frame.accel_x_g = sample.accel_x_g;
  g_frame.accel_y_g = sample.accel_y_g;
  g_frame.accel_z_g = sample.accel_z_g;
  g_frame.gyro_x_dps = sample.gyro_x_dps;
  g_frame.gyro_y_dps = sample.gyro_y_dps;
  g_frame.gyro_z_dps = sample.gyro_z_dps;
  g_frame.latitude_deg = sample.latitude_deg;
  g_frame.longitude_deg = sample.longitude_deg;
  g_frame.gps_altitude_m = sample.gps_altitude_m;
  g_frame.satellites = sample.satellites;
  g_frame.gps_fix = sample.gps_fix;
  g_frame.mag_x_ut = sample.mag_x_ut;
  g_frame.mag_y_ut = sample.mag_y_ut;
  g_frame.mag_z_ut = sample.mag_z_ut;
  for (uint8_t i = 0; i < 4; ++i) {
    g_frame.optical_raw[i] = sample.optical_raw[i];
  }
  g_frame.rtc_hour = sample.rtc_hour;
  g_frame.rtc_minute = sample.rtc_minute;
  g_frame.rtc_second = sample.rtc_second;
  g_frame.rtc_valid = sample.rtc_valid;
}

void persist_state(uint32_t mission_time_ms) {
  g_persistent.magic = cansat::kPersistentMagic;
  g_persistent.version = cansat::kPersistentVersion;
  g_persistent.packet_count = g_packet_count;
  g_persistent.mission_time_ms = mission_time_ms;
  g_persistent.state = g_fsm.state();
  g_persistent.armed = g_fsm.armed();
  g_persistent.second_parachute_deployed =
      g_apogee.second_parachute_triggered() || g_mechanism_activated;
  storage_save_state(g_persistent);
}

void update_flight_logic(uint32_t now_ms) {
  if (!g_fsm.armed()) {
    g_fsm.on_event(Event::Arm);
  }

  const float alt_m = g_frame.altitude_m;
  if (g_fsm.state() == State::LaunchPad && alt_m >= 15.0f) {
    if (g_fsm.on_event(Event::LaunchDetected)) {
      g_launch_start_ms = now_ms;
      trigger_xiao_capture(0x01);  // Inicio de ascenso
      // Requisito X3: enviar comando automatico cuando el CanSat comienza la mision
      Serial.println("CMD:MISSION_START;STATE:Ascent");
      Serial2.println("CMD:MISSION_START;STATE:Ascent");
      Serial.println("EVENT: LAUNCH_DETECTED -> Ascent");
    }
  }

  if (g_fsm.state() == State::Ascent || g_fsm.state() == State::Descent) {
    const uint32_t flight_elapsed_ms =
        (now_ms >= g_launch_start_ms) ? (now_ms - g_launch_start_ms) : 0;
    const FlightEvent fev = g_apogee.update(alt_m, flight_elapsed_ms);

    if (fev == FlightEvent::ApogeeDetected) {
      g_fsm.on_event(Event::ApogeeDetected);
      trigger_xiao_capture(0x02);  // Disparo de camaras en apogeo/descenso
      Serial.print("EVENT: APOGEE_DETECTED -> Descent (Max Alt: ");
      Serial.print(g_apogee.max_altitude_m(), 1);
      Serial.println(" m)");
    } else if (fev == FlightEvent::SecondParachuteTriggered) {
      g_mechanism_activated = true;
      trigger_xiao_capture(0x03);  // Disparo en apertura de 2do paracaidas
      Serial.print(
          "EVENT: SECOND_PARACHUTE_80_PERCENT -> Servo 90 deg (Alt: ");
      Serial.print(alt_m, 1);
      Serial.println(" m)");
    }
  }

  if (g_fsm.state() == State::Descent && alt_m <= 5.0f) {
    if (g_fsm.on_event(Event::LandedDetected)) {
      trigger_xiao_capture(0x00);  // Fin de adquisicion
      Serial.println("EVENT: LANDED_DETECTED -> Landed");
    }
  }
}

void parse_st_time(const char* line, uint32_t now_ms) {
  // Soporta "ST HH:MM:SS" o "ST YYYY-MM-DDTHH:MM:SS"
  const char* p = std::strchr(line, 'T');
  if (p != nullptr) {
    ++p;
  } else if (std::strlen(line) > 3) {
    p = line + 3;
  }
  if (p != nullptr) {
    int h = 0;
    int m = 0;
    int s = 0;
    if (std::sscanf(p, "%d:%d:%d", &h, &m, &s) == 3) {
      g_clock.sync_hms(static_cast<uint8_t>(h), static_cast<uint8_t>(m),
                       static_cast<uint8_t>(s), now_ms);
    }
  }
}

void handle_command(const char* line, uint32_t now_ms) {
  if (std::strncmp(line, "RST", 3) == 0) {
    g_fsm.on_event(Event::Reset);
    g_fsm.on_event(Event::Arm);
    g_apogee.reset();
    g_mechanism_activated = false;
    std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "RST");
    Serial.println("RST OK");
    return;
  }
  if (std::strncmp(line, "$RPI,", 5) == 0) {
    char copy[160];
    std::snprintf(copy, sizeof(copy), "%s", line);
    parse_rpi_line(copy, now_ms);
    return;
  }

  const CommandType command = cansat::parse_command(line);
  switch (command) {
    case CommandType::Status: {
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "CX");
      char resp[192];
      std::snprintf(
          resp, sizeof(resp),
          "CX OK %s PKT=%lu BATT=%.2fV CURR=%.1fmA SOLAR=%u,%u,%u,%u",
          FlightStateMachine::state_name(g_fsm.state()),
          static_cast<unsigned long>(g_packet_count), g_frame.battery_v,
          g_frame.current_ma,
          static_cast<unsigned>(g_frame.optical_raw[0]),
          static_cast<unsigned>(g_frame.optical_raw[1]),
          static_cast<unsigned>(g_frame.optical_raw[2]),
          static_cast<unsigned>(g_frame.optical_raw[3]));
      Serial.println(resp);
      Serial2.println(resp);
      break;
    }
    case CommandType::SetTime:
      parse_st_time(line, now_ms);
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "ST");
      Serial.println("ST OK");
      break;
    case CommandType::CalibrateAltitude:
      if (g_baro_last_pa > 0.0f) {
        g_baro_baseline_pa = g_baro_last_pa;
        g_baro_altitude_m = 0.0f;
      } else {
        g_persistent.altitude_offset_m += g_frame.altitude_m;
      }
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "CAL");
      persist_state(g_frame.mission_time_ms);
      Serial.println("CAL OK");
      break;
    case CommandType::ActivateMechanism:
      g_mechanism_activated = true;
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "MEC");
      persist_state(g_frame.mission_time_ms);
      Serial.println("MEC OK (Servo liberado -> 90 deg)");
      break;
    case CommandType::None:
      break;
  }
}

}  // namespace

void setup() {
  Serial.begin(cansat::kSerialBaud);

  pinMode(cansat::kPinMpu6050Int, INPUT);
  pinMode(cansat::kPinServoV3, OUTPUT);

  // Inicializa bus I2C en GPIO 8/9 y verifica si los sensores estan ahi o en
  // el bus alternativo (GPIO 13/14) cuando el BMP280 ocupa GPIO 8/9 por SPI.
  Wire.begin(cansat::kI2cSdaPin, cansat::kI2cSclPin, cansat::kI2cClockHz);
  g_i2c_bus = &Wire;

  if (!i2c_probe_on(Wire, cansat::kMpu6050I2cAddress) &&
      !i2c_probe_on(Wire, cansat::kIna219I2cAddress) &&
      !i2c_probe_on(Wire, cansat::kFramI2cAddress)) {
    Wire1.begin(cansat::kI2cAltSdaPin, cansat::kI2cAltSclPin,
                cansat::kI2cClockHz);
    if (i2c_probe_on(Wire1, cansat::kMpu6050I2cAddress) ||
        i2c_probe_on(Wire1, cansat::kIna219I2cAddress) ||
        i2c_probe_on(Wire1, cansat::kFramI2cAddress)) {
      g_i2c_bus = &Wire1;
    }
  }

  g_mpu_available = init_mpu6050();
  g_baro_available = init_barometer();
  g_ina_available = init_ina219();
  g_fram_available = init_fram();
  g_lis_available = init_lis3mdl();
  g_ads_available = init_ads1115();
  init_xiao_cameras();

  // UART1: GPS NEO-M8N (9600 8N1)
  Serial1.begin(cansat::kGpsBaud, SERIAL_8N1, cansat::kGpsRxPin,
                cansat::kGpsTxPin);

  // UART2: XBee S1 Module + Raspberry Pi Zero W (115200 8N1)
  Serial2.begin(cansat::kRadioAndRpiBaud, SERIAL_8N1, cansat::kRadioAndRpiRxPin,
                cansat::kRadioAndRpiTxPin);

  if (g_baro_available) {
    float p = 0.0f;
    float t = 0.0f;
    float a = 0.0f;
    if (read_barometer(p, t, a) && p > 0.0f) {
      g_baro_baseline_pa = p;
    }
  }

  PersistentState restored{};
  if (storage_load_state(restored)) {
    g_packet_count = restored.packet_count;
    g_mission_time_offset_ms = restored.mission_time_ms;
    g_mechanism_activated = restored.second_parachute_deployed;
    g_fsm.restore(restored.state, restored.armed);
    g_persistent = restored;
  } else {
    g_fsm.on_event(Event::Arm);
  }

  g_last_telemetry_ms = millis();

  Serial.println("=========================================================");
  Serial.println("CanSat UTN FRRe - OBC ESP32-S3 (Circuito v3 - ISI 1)");
  Serial.printf(
      "IMU:%s BARO:%s INA219:%s FRAM:%s LIS3MDL:%s ADS1115:%s XIAO:%u/%u\n",
      g_mpu_available ? "MPU-6050" : "FAIL",
      g_baro.chip_id == 0x58
          ? (g_baro.transport == BaroTransport::Spi ? "BMP280-SPI"
                                                    : "BMP280-I2C")
          : "FAIL",
      g_ina_available ? "OK" : "FAIL", g_fram_available ? "OK" : "RAM",
      g_lis_available ? "I2C" : "VIA-RPI",
      g_ads_available ? "I2C" : "VIA-RPI",
      g_xiao_vis_available ? 1u : 0u, g_xiao_nir_available ? 1u : 0u);
  Serial.println("=========================================================");
}

void loop() {
  const uint32_t now = millis();

  SensorSample sample{};
  read_mpu6050(sample);

  g_ina_available =
      g_ina_available && read_ina219(sample.battery_v, sample.current_ma);
  if (!g_ina_available) {
    sample.battery_v = 0.0f;
    sample.current_ma = 0.0f;
  }

  poll_gps(now);
  poll_radio_and_rpi(now);
  poll_xiao_status();

  sample.gps_fix = g_gps.fix ? 1u : 0u;
  sample.satellites = g_gps.satellites;
  sample.latitude_deg = g_gps.latitude_deg;
  sample.longitude_deg = g_gps.longitude_deg;
  sample.gps_altitude_m = g_gps.altitude_m;

  // Tiempo de mision / UTC
  g_clock.get_hms(now, sample.rtc_hour, sample.rtc_minute, sample.rtc_second,
                  sample.rtc_valid);

  read_magnetometer_and_optical(sample, now);

  float baro_pressure = 0.0f;
  float baro_temperature = 0.0f;
  const bool baro_ok =
      read_barometer(baro_pressure, baro_temperature, g_baro_altitude_m);
  g_baro_available = baro_ok;

  if (baro_ok) {
    sample.altitude_m = g_baro_altitude_m;
    sample.pressure_pa = baro_pressure;
    if (!g_mpu_available) {
      sample.temperature_c = baro_temperature;
    }
  } else {
    sample.altitude_m = 0.0f;
    sample.pressure_pa = 0.0f;
  }

  apply_sample(sample);
  update_flight_logic(now);
  pulse_servo(now);

  if (Serial.available() > 0) {
    char command_line[96];
    const std::size_t length =
        Serial.readBytesUntil('\n', command_line, sizeof(command_line) - 1);
    command_line[length] = '\0';
    handle_command(command_line, now);
  }

  if ((now - g_last_telemetry_ms) >= cansat::kTelemetryPeriodMs) {
    g_last_telemetry_ms += cansat::kTelemetryPeriodMs;
    g_packet_count = cansat::next_packet_count(g_packet_count);

    if (g_rpi_payload_summary[0] != '\0') {
      std::snprintf(
          g_optional_buffer, sizeof(g_optional_buffer),
          "MAX:%.1f;P2:%u;CMD:%s;RPI:%s", g_apogee.max_altitude_m(),
          (g_apogee.second_parachute_triggered() || g_mechanism_activated) ? 1u
                                                                           : 0u,
          g_last_cmd_echo, g_rpi_payload_summary);
    } else {
      std::snprintf(
          g_optional_buffer, sizeof(g_optional_buffer),
          "MAX:%.1f;P2:%u;CMD:%s", g_apogee.max_altitude_m(),
          (g_apogee.second_parachute_triggered() || g_mechanism_activated) ? 1u
                                                                           : 0u,
          g_last_cmd_echo);
    }

    const uint32_t mission_time_ms = g_mission_time_offset_ms + now;
    g_frame.team_id = cansat::kTeamId;
    g_frame.mission_time_ms = mission_time_ms;
    g_frame.packet_count = g_packet_count;
    g_frame.mode = 0;
    g_frame.state = g_fsm.state();
    g_frame.optional_data = g_optional_buffer;

    char line[384];
    cansat::format_telemetry_csv(g_frame, line, sizeof(line));
    Serial.println(line);
    // Envia la trama de telemetria tambien por UART2 al XBee S1 y a la RPi Zero
    Serial2.println(line);

    persist_state(mission_time_ms);
  }
}
