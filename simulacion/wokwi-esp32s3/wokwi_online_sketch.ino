// ============================================================================
// CanSat Universitario 2026-2027 — UTN FRRe
// Autora: Florencia Gonzalez (ISI 1)
// Archivo de simulacion aislado para Wokwi Online (https://wokwi.com/projects/new/esp32-s3)
// Ubicacion: simulacion/wokwi-esp32s3/wokwi_online_sketch.ino
// (Aislado completamente del codigo funcional de vuelo en firmware/)
// ============================================================================

#define CANSAT_SIMULATION 1

#include <Arduino.h>
#include <Wire.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cansat {

constexpr const char* kBoardName = "ESP32-S3 DevKitC-1";
constexpr uint16_t kTeamId = 1000;
constexpr uint32_t kTelemetryPeriodMs = 1000;
constexpr uint32_t kSerialBaud = 115200;

constexpr uint8_t kI2cSdaPin = 8;
constexpr uint8_t kI2cSclPin = 9;
constexpr uint32_t kI2cClockHz = 400000;

constexpr uint8_t kPinServoV3 = 7;
constexpr uint8_t kPinMpu6050Int = 14;

constexpr uint8_t kGpsRxPin = 18;
constexpr uint8_t kGpsTxPin = 17;
constexpr uint32_t kGpsBaud = 9600;

constexpr float kMinApogeeAltitudeM = 100.0f;
constexpr float kApogeeDropM = 5.0f;
constexpr float kSecondParachuteRatio = 0.80f;
constexpr uint32_t kMinFlightTimeMs = 10000;

constexpr uint8_t kFramI2cAddress = 0x50;
constexpr uint8_t kMpu6050I2cAddress = 0x68;
constexpr uint8_t kBmp280I2cAddress = 0x76;
constexpr uint8_t kBmp280AltI2cAddress = 0x77;
constexpr uint8_t kLis3mdlI2cAddress = 0x1C;
constexpr uint8_t kIna219I2cAddress = 0x40;
constexpr uint8_t kAds1115I2cAddress = 0x48;

constexpr float kIna219ShuntOhms = 0.1f;

constexpr uint8_t kPinOptical1 = 1;
constexpr uint8_t kPinOptical2 = 2;
constexpr uint8_t kPinOptical3 = 3;
constexpr uint8_t kPinOptical4 = 6;
constexpr uint8_t kPinAltitudePot = 4;
constexpr uint8_t kPinLaunchButton = 5;
constexpr uint8_t kPinLedLaunchPad = 10;
constexpr uint8_t kPinLedAscent = 11;
constexpr uint8_t kPinLedDescent = 12;
constexpr uint8_t kPinLedSecondParachute = 13;
constexpr uint8_t kPinLedLanded = 14;
constexpr uint8_t kPinServoMechanism = 15;

enum class State : uint8_t {
  LaunchPad = 0,
  Ascent = 1,
  Descent = 2,
  Landed = 3,
};

enum class Event : uint8_t {
  Arm = 0,
  LaunchDetected = 1,
  ApogeeDetected = 2,
  LandedDetected = 3,
  Reset = 4,
};

class FlightStateMachine {
 public:
  FlightStateMachine() : state_(State::LaunchPad), armed_(false) {}

  State state() const { return state_; }
  bool armed() const { return armed_; }

  bool on_event(Event event) {
    switch (event) {
      case Event::Arm:
        armed_ = true;
        return true;
      case Event::LaunchDetected:
        if (armed_ && state_ == State::LaunchPad) {
          return transition_to(State::Ascent);
        }
        return false;
      case Event::ApogeeDetected:
        if (state_ == State::Ascent) {
          return transition_to(State::Descent);
        }
        return false;
      case Event::LandedDetected:
        if (state_ == State::Descent) {
          return transition_to(State::Landed);
        }
        return false;
      case Event::Reset:
        armed_ = false;
        return transition_to(State::LaunchPad);
    }
    return false;
  }

  void restore(State state, bool armed) {
    state_ = state;
    armed_ = armed;
  }

  static const char* state_name(State state) {
    switch (state) {
      case State::LaunchPad:
        return "LaunchPad";
      case State::Ascent:
        return "Ascent";
      case State::Descent:
        return "Descent";
      case State::Landed:
        return "Landed";
    }
    return "Unknown";
  }

 private:
  bool transition_to(State next) {
    if (state_ == next) return false;
    state_ = next;
    return true;
  }

  State state_;
  bool armed_;
};

enum class FlightEvent : uint8_t {
  None = 0,
  ApogeeDetected = 1,
  SecondParachuteTriggered = 2,
};

struct ApogeeConfig {
  float min_altitude_m = 100.0f;
  float apogee_drop_m = 5.0f;
  float second_parachute_ratio = 0.80f;
  uint32_t min_flight_time_ms = 10000;
};

class ApogeeDetector {
 public:
  explicit ApogeeDetector(const ApogeeConfig& config = ApogeeConfig{})
      : config_(config) {}

  FlightEvent update(float altitude_m, uint32_t timestamp_ms) {
    if (timestamp_ms < config_.min_flight_time_ms) return FlightEvent::None;
    if (altitude_m > max_altitude_m_) {
      max_altitude_m_ = altitude_m;
      max_altitude_time_ms_ = timestamp_ms;
      return FlightEvent::None;
    }
    if (max_altitude_m_ < config_.min_altitude_m) return FlightEvent::None;
    if (!apogee_detected_) {
      if ((max_altitude_m_ - altitude_m) >= config_.apogee_drop_m) {
        apogee_detected_ = true;
        return FlightEvent::ApogeeDetected;
      }
      return FlightEvent::None;
    }
    if (!second_parachute_triggered_) {
      const float threshold = max_altitude_m_ * config_.second_parachute_ratio;
      if (altitude_m <= threshold) {
        second_parachute_triggered_ = true;
        return FlightEvent::SecondParachuteTriggered;
      }
    }
    return FlightEvent::None;
  }

  void reset() {
    max_altitude_m_ = 0.0f;
    max_altitude_time_ms_ = 0;
    apogee_detected_ = false;
    second_parachute_triggered_ = false;
  }

  float max_altitude_m() const { return max_altitude_m_; }
  bool apogee_detected() const { return apogee_detected_; }
  bool second_parachute_triggered() const {
    return second_parachute_triggered_;
  }

 private:
  ApogeeConfig config_;
  float max_altitude_m_ = 0.0f;
  uint32_t max_altitude_time_ms_ = 0;
  bool apogee_detected_ = false;
  bool second_parachute_triggered_ = false;
};

constexpr uint32_t kPersistentMagic = 0x43414E53;
constexpr uint8_t kPersistentVersion = 1;

struct PersistentState {
  uint32_t magic = kPersistentMagic;
  uint8_t version = kPersistentVersion;
  uint32_t packet_count = 0;
  uint32_t mission_time_ms = 0;
  float altitude_offset_m = 0.0f;
  State state = State::LaunchPad;
  bool armed = false;
  bool second_parachute_deployed = false;
  uint32_t checksum = 0;
};

inline void mix_u32(uint32_t& hash, uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    hash ^= (value >> shift) & 0xFFu;
    hash *= 16777619u;
  }
}

inline void mix_float(uint32_t& hash, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  mix_u32(hash, bits);
}

inline uint32_t compute_checksum(const PersistentState& state) {
  uint32_t hash = 2166136261u;
  mix_u32(hash, state.magic);
  mix_u32(hash, state.version);
  mix_u32(hash, state.packet_count);
  mix_u32(hash, state.mission_time_ms);
  mix_float(hash, state.altitude_offset_m);
  mix_u32(hash, static_cast<uint32_t>(state.state));
  mix_u32(hash, state.armed ? 1u : 0u);
  mix_u32(hash, state.second_parachute_deployed ? 1u : 0u);
  return hash;
}

class MemoryStorage {
 public:
  bool load(PersistentState& out) {
    if (!has_state_) return false;
    out = state_;
    return out.checksum == compute_checksum(out);
  }
  bool save(const PersistentState& state) {
    state_ = state;
    state_.checksum = compute_checksum(state_);
    has_state_ = true;
    return true;
  }

 private:
  PersistentState state_{};
  bool has_state_ = false;
};

struct SensorSample {
  uint32_t timestamp_ms = 0;
  float altitude_m = 0.0f;
  float pressure_pa = 0.0f;
  float temperature_c = 0.0f;
  float battery_v = 0.0f;
  float current_ma = 0.0f;
  float accel_x_g = 0.0f;
  float accel_y_g = 0.0f;
  float accel_z_g = 0.0f;
  float gyro_x_dps = 0.0f;
  float gyro_y_dps = 0.0f;
  float gyro_z_dps = 0.0f;
  float latitude_deg = 0.0f;
  float longitude_deg = 0.0f;
  float gps_altitude_m = 0.0f;
  uint8_t satellites = 0;
  uint8_t gps_fix = 0;
  float mag_x_ut = 0.0f;
  float mag_y_ut = 0.0f;
  float mag_z_ut = 0.0f;
  uint16_t optical_raw[4] = {0, 0, 0, 0};
  uint8_t rtc_hour = 0;
  uint8_t rtc_minute = 0;
  uint8_t rtc_second = 0;
  uint8_t rtc_valid = 0;
};

struct TelemetryFrame {
  uint16_t team_id = 0;
  uint32_t mission_time_ms = 0;
  uint32_t packet_count = 0;
  uint8_t mode = 0;
  State state = State::LaunchPad;
  float altitude_m = 0.0f;
  float pressure_pa = 0.0f;
  float temperature_c = 0.0f;
  float battery_v = 0.0f;
  float current_ma = 0.0f;
  float accel_x_g = 0.0f;
  float accel_y_g = 0.0f;
  float accel_z_g = 0.0f;
  float gyro_x_dps = 0.0f;
  float gyro_y_dps = 0.0f;
  float gyro_z_dps = 0.0f;
  float latitude_deg = 0.0f;
  float longitude_deg = 0.0f;
  float gps_altitude_m = 0.0f;
  uint8_t satellites = 0;
  uint8_t gps_fix = 0;
  float mag_x_ut = 0.0f;
  float mag_y_ut = 0.0f;
  float mag_z_ut = 0.0f;
  uint16_t optical_raw[4] = {0, 0, 0, 0};
  uint8_t rtc_hour = 0;
  uint8_t rtc_minute = 0;
  uint8_t rtc_second = 0;
  uint8_t rtc_valid = 0;
  const char* optional_data = "";
};

std::size_t format_telemetry_csv(const TelemetryFrame& frame, char* buffer,
                                 std::size_t buffer_size) {
  if (buffer == nullptr || buffer_size == 0) return 0;
  const int written = std::snprintf(
      buffer, buffer_size,
      "%u,%lu,%lu,%u,%s,%.2f,%.2f,%.2f,%.2f,%.2f,"
      "%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.6f,%.6f,%.2f,%u,"
      "%.1f,%.1f,%.1f,%u,%u,%u,%u,%u,%u,%u,%u,%u,%s",
      static_cast<unsigned>(frame.team_id),
      static_cast<unsigned long>(frame.mission_time_ms),
      static_cast<unsigned long>(frame.packet_count),
      static_cast<unsigned>(frame.mode),
      FlightStateMachine::state_name(frame.state), frame.altitude_m,
      frame.pressure_pa, frame.temperature_c, frame.battery_v, frame.current_ma,
      frame.accel_x_g, frame.accel_y_g, frame.accel_z_g, frame.gyro_x_dps,
      frame.gyro_y_dps, frame.gyro_z_dps, frame.latitude_deg,
      frame.longitude_deg, frame.gps_altitude_m,
      static_cast<unsigned>(frame.satellites), frame.mag_x_ut, frame.mag_y_ut,
      frame.mag_z_ut, static_cast<unsigned>(frame.optical_raw[0]),
      static_cast<unsigned>(frame.optical_raw[1]),
      static_cast<unsigned>(frame.optical_raw[2]),
      static_cast<unsigned>(frame.optical_raw[3]),
      static_cast<unsigned>(frame.rtc_hour),
      static_cast<unsigned>(frame.rtc_minute),
      static_cast<unsigned>(frame.rtc_second),
      static_cast<unsigned>(frame.rtc_valid),
      static_cast<unsigned>(frame.gps_fix),
      frame.optional_data == nullptr ? "" : frame.optional_data);
  if (written < 0) {
    buffer[0] = '\0';
    return 0;
  }
  const std::size_t length = static_cast<std::size_t>(written);
  return length < buffer_size ? length : buffer_size - 1;
}

uint32_t next_packet_count(uint32_t current) { return current + 1; }

enum class CommandType : uint8_t {
  None = 0,
  Status = 1,
  SetTime = 2,
  CalibrateAltitude = 3,
  ActivateMechanism = 4,
};

bool matches_cmd(const char* line, const char* token) {
  const std::size_t length = std::strlen(token);
  if (std::strncmp(line, token, length) != 0) return false;
  const char next = line[length];
  return next == '\0' || next == ' ' || next == '\r' || next == '\n';
}

CommandType parse_command(const char* line) {
  if (line == nullptr) return CommandType::None;
  if (matches_cmd(line, "CX")) return CommandType::Status;
  if (matches_cmd(line, "ST")) return CommandType::SetTime;
  if (matches_cmd(line, "CAL")) return CommandType::CalibrateAltitude;
  if (matches_cmd(line, "MEC")) return CommandType::ActivateMechanism;
  return CommandType::None;
}

}  // namespace cansat

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
uint32_t g_last_telemetry_ms = 0;
uint32_t g_last_servo_pulse_ms = 0;
uint32_t g_launch_start_ms = 0;
uint32_t g_mission_time_offset_ms = 0;

bool g_mpu_available = false;
bool g_baro_available = false;
bool g_ina_available = false;
bool g_fram_available = false;
bool g_lis_available = false;
bool g_ads_available = false;

bool g_auto_flight_active = false;
bool g_manual_altitude = false;
uint32_t g_auto_flight_start_ms = 0;
bool g_mechanism_activated = false;
bool g_last_button_state = true;
int g_last_pot_raw = -1;
char g_last_cmd_echo[16] = "NONE";
char g_rpi_payload_summary[64] = "";
char g_optional_buffer[160] = "";

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

struct RpiPayloadState {
  bool valid = false;
  uint32_t last_rx_ms = 0;
  float mag_x_ut = 0.0f;
  float mag_y_ut = 0.0f;
  float mag_z_ut = 0.0f;
  uint16_t optical_raw[4] = {0, 0, 0, 0};
};

RpiPayloadState g_rpi;
TwoWire* g_i2c_bus = &Wire;

bool i2c_probe_on(TwoWire& bus, uint8_t address) {
  bus.beginTransmission(address);
  return bus.endTransmission() == 0;
}

bool i2c_read_on(TwoWire& bus, uint8_t address, uint8_t reg, uint8_t* buffer,
                 uint8_t length) {
  bus.beginTransmission(address);
  bus.write(reg);
  if (bus.endTransmission(false) != 0) return false;
  if (bus.requestFrom(address, length) != length) return false;
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

bool init_mpu6050() {
  if (!i2c_write_u8(cansat::kMpu6050I2cAddress, 0x6B, 0x00)) return false;
  i2c_write_u8(cansat::kMpu6050I2cAddress, 0x38, 0x01);
  return true;
}

bool read_mpu6050(SensorSample& sample) {
  if (!g_mpu_available) return false;
  uint8_t raw[14];
  if (!i2c_read(cansat::kMpu6050I2cAddress, 0x3B, raw, sizeof(raw))) {
    return false;
  }
  sample.accel_x_g = static_cast<float>(be_s16(raw + 0)) / 16384.0f;
  sample.accel_y_g = static_cast<float>(be_s16(raw + 2)) / 16384.0f;
  sample.accel_z_g = static_cast<float>(be_s16(raw + 4)) / 16384.0f;
  sample.temperature_c =
      (static_cast<float>(be_s16(raw + 6)) / 340.0f) + 36.53f;
  sample.gyro_x_dps = static_cast<float>(be_s16(raw + 8)) / 131.0f;
  sample.gyro_y_dps = static_cast<float>(be_s16(raw + 10)) / 131.0f;
  sample.gyro_z_dps = static_cast<float>(be_s16(raw + 12)) / 131.0f;
  return true;
}

struct Barometer {
  uint8_t chip_id = 0;
  uint8_t address = 0;
  int32_t t_fine = 0;
  int16_t ac1 = 0, ac2 = 0, ac3 = 0;
  uint16_t ac4 = 0, ac5 = 0, ac6 = 0;
  int16_t b1 = 0, b2 = 0, mb = 0, mc = 0, md = 0;
};

Barometer g_baro;
float g_baro_baseline_pa = 101325.0f;
float g_baro_altitude_m = 0.0f;
float g_baro_last_pa = 0.0f;

bool init_bmp180(uint8_t address) {
  uint8_t cal[22];
  if (!i2c_read(address, 0xAA, cal, sizeof(cal))) return false;
  g_baro.ac1 = be_s16(cal + 0);
  g_baro.ac2 = be_s16(cal + 2);
  g_baro.ac3 = be_s16(cal + 4);
  g_baro.ac4 = be_u16(cal + 6);
  g_baro.ac5 = be_u16(cal + 8);
  g_baro.ac6 = be_u16(cal + 10);
  g_baro.b1 = be_s16(cal + 12);
  g_baro.b2 = be_s16(cal + 14);
  g_baro.mb = be_s16(cal + 16);
  g_baro.mc = be_s16(cal + 18);
  g_baro.md = be_s16(cal + 20);
  g_baro.address = address;
  g_baro.chip_id = 0x55;
  return true;
}

bool init_barometer() {
  uint8_t id = 0;
  if (i2c_read(0x77, 0xD0, &id, 1) && id == 0x55) {
    return init_bmp180(0x77);
  }
  return false;
}

float bmp180_temperature_c() {
  if (!i2c_write_u8(g_baro.address, 0xF4, 0x2E)) return NAN;
  delay(5);
  uint8_t raw[2];
  if (!i2c_read(g_baro.address, 0xF6, raw, 2)) return NAN;
  const int32_t ut = static_cast<int32_t>(be_u16(raw));
  const int32_t x1 =
      ((ut - static_cast<int32_t>(g_baro.ac6)) * g_baro.ac5) >> 15;
  const int32_t divisor = x1 + g_baro.md;
  if (divisor == 0) return NAN;
  const int32_t x2 = (static_cast<int32_t>(g_baro.mc) << 11) / divisor;
  g_baro.t_fine = x1 + x2;
  return static_cast<float>((g_baro.t_fine + 8) >> 4) / 10.0f;
}

float bmp180_pressure_pa() {
  if (!i2c_write_u8(g_baro.address, 0xF4, 0x34)) return NAN;
  delay(5);
  uint8_t raw[3];
  if (!i2c_read(g_baro.address, 0xF6, raw, 3)) return NAN;
  const int32_t up = static_cast<int32_t>(
      ((static_cast<uint32_t>(raw[0]) << 16) |
       (static_cast<uint32_t>(raw[1]) << 8) | raw[2]) >>
      8);
  int32_t b6 = g_baro.t_fine - 4000;
  int32_t x1 = (g_baro.b2 * ((b6 * b6) >> 12)) >> 11;
  int32_t x2 = (g_baro.ac2 * b6) >> 11;
  int32_t x3 = x1 + x2;
  int32_t b3 = (((static_cast<int32_t>(g_baro.ac1) * 4 + x3) << 0) + 2) / 4;
  x1 = (g_baro.ac3 * b6) >> 13;
  x2 = (g_baro.b1 * ((b6 * b6) >> 12)) >> 16;
  x3 = ((x1 + x2) + 2) >> 2;
  const uint32_t b4 =
      (static_cast<uint32_t>(g_baro.ac4) * static_cast<uint32_t>(x3 + 32768)) >>
      15;
  if (b4 == 0) return NAN;
  const int64_t b7 =
      (static_cast<int64_t>(up) - static_cast<int64_t>(b3)) * 50000;
  int32_t p = static_cast<int32_t>((b7 * 2) / static_cast<int64_t>(b4));
  x1 = (p >> 8) * (p >> 8);
  x1 = (x1 * 3038) >> 16;
  x2 = (-7357 * p) >> 16;
  p = p + ((x1 + x2 + 3791) >> 4);
  return static_cast<float>(p);
}

bool read_barometer(float& pressure_pa, float& temperature_c,
                    float& altitude_m) {
  if (!g_baro.chip_id) return false;
  const float t = bmp180_temperature_c();
  if (std::isnan(t)) return false;
  const float p = bmp180_pressure_pa();
  if (std::isnan(p) || p <= 0.0f) return false;
  temperature_c = t;
  pressure_pa = p;
  g_baro_last_pa = pressure_pa;
  const float ratio = pressure_pa / g_baro_baseline_pa;
  altitude_m = 44330.0f * (1.0f - std::pow(ratio, 1.0f / 5.255f));
  return true;
}

float compute_auto_flight_altitude(uint32_t elapsed_ms) {
  const float t_sec = static_cast<float>(elapsed_ms) / 1000.0f;
  if (t_sec <= 15.0f) return (t_sec / 15.0f) * 400.0f;
  if (t_sec <= 32.0f) {
    const float descent_ratio = (t_sec - 15.0f) / 17.0f;
    const float alt = 400.0f * (1.0f - descent_ratio);
    return alt > 0.0f ? alt : 0.0f;
  }
  return 0.0f;
}

float pressure_from_altitude(float altitude_m) {
  return 101325.0f * std::pow(1.0f - (altitude_m / 44330.0f), 5.255f);
}

void update_visual_outputs() {
  const State current = g_fsm.state();
  digitalWrite(cansat::kPinLedLaunchPad,
               current == State::LaunchPad ? HIGH : LOW);
  digitalWrite(cansat::kPinLedAscent, current == State::Ascent ? HIGH : LOW);
  digitalWrite(cansat::kPinLedDescent, current == State::Descent ? HIGH : LOW);
  digitalWrite(cansat::kPinLedLanded, current == State::Landed ? HIGH : LOW);
  const bool para2_open =
      g_apogee.second_parachute_triggered() || g_mechanism_activated;
  digitalWrite(cansat::kPinLedSecondParachute, para2_open ? HIGH : LOW);
}

void pulse_servo(uint32_t now_ms) {
  if ((now_ms - g_last_servo_pulse_ms) < 20) return;
  g_last_servo_pulse_ms = now_ms;
  const bool para2_open =
      g_apogee.second_parachute_triggered() || g_mechanism_activated;
  const int angle_deg = para2_open ? 90 : 0;
  const int pulse_us = 544 + (angle_deg * (2400 - 544)) / 180;
  digitalWrite(cansat::kPinServoV3, HIGH);
  digitalWrite(cansat::kPinServoMechanism, HIGH);
  delayMicroseconds(pulse_us);
  digitalWrite(cansat::kPinServoV3, LOW);
  digitalWrite(cansat::kPinServoMechanism, LOW);
}

void start_auto_flight(uint32_t now_ms) {
  g_fsm.on_event(Event::Reset);
  g_fsm.on_event(Event::Arm);
  g_apogee.reset();
  g_mechanism_activated = false;
  g_auto_flight_active = true;
  g_manual_altitude = false;
  g_auto_flight_start_ms = now_ms;
  g_launch_start_ms = now_ms;
  std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "SIM");
  Serial.println(
      "EVENT: AUTO_FLIGHT_START (0m -> 400m -> Apogeo -> 80% -> Landed)");
}

void update_flight_logic(uint32_t now_ms) {
  if (!g_fsm.armed()) g_fsm.on_event(Event::Arm);
  const float alt_m = g_frame.altitude_m;
  if (g_fsm.state() == State::LaunchPad && alt_m >= 15.0f) {
    if (g_fsm.on_event(Event::LaunchDetected)) {
      g_launch_start_ms = now_ms;
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "START");
      Serial.println("CMD:MISSION_START;STATE:Ascent");
      Serial.println("EVENT: LAUNCH_DETECTED -> Ascent");
    }
  }
  if (g_fsm.state() == State::Ascent || g_fsm.state() == State::Descent) {
    const uint32_t flight_elapsed_ms =
        (now_ms >= g_launch_start_ms) ? (now_ms - g_launch_start_ms) : 0;
    const FlightEvent fev = g_apogee.update(alt_m, flight_elapsed_ms);
    if (fev == FlightEvent::ApogeeDetected) {
      g_fsm.on_event(Event::ApogeeDetected);
      Serial.print("EVENT: APOGEE_DETECTED -> Descent (Max Alt: ");
      Serial.print(g_apogee.max_altitude_m(), 1);
      Serial.println(" m)");
    } else if (fev == FlightEvent::SecondParachuteTriggered) {
      g_mechanism_activated = true;
      Serial.print(
          "EVENT: SECOND_PARACHUTE_80_PERCENT -> Servo 90 deg (Alt: ");
      Serial.print(alt_m, 1);
      Serial.println(" m)");
    }
  }
  if (g_fsm.state() == State::Descent && alt_m <= 5.0f) {
    if (g_fsm.on_event(Event::LandedDetected)) {
      g_auto_flight_active = false;
      Serial.println("EVENT: LANDED_DETECTED -> Landed");
    }
  }
}

void parse_st_time(const char* line, uint32_t now_ms) {
  const char* p = std::strchr(line, 'T');
  if (p != nullptr) {
    ++p;
  } else if (std::strlen(line) > 3) {
    p = line + 3;
  }
  if (p != nullptr) {
    int h = 0, m = 0, s = 0;
    if (std::sscanf(p, "%d:%d:%d", &h, &m, &s) == 3) {
      g_clock.sync_hms(static_cast<uint8_t>(h), static_cast<uint8_t>(m),
                       static_cast<uint8_t>(s), now_ms);
    }
  }
}

void handle_command(const char* line, uint32_t now_ms) {
  if (std::strncmp(line, "SIM", 3) == 0) {
    start_auto_flight(now_ms);
    return;
  }
  if (std::strncmp(line, "RST", 3) == 0) {
    g_fsm.on_event(Event::Reset);
    g_fsm.on_event(Event::Arm);
    g_apogee.reset();
    g_mechanism_activated = false;
    g_auto_flight_active = false;
    g_manual_altitude = false;
    std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "RST");
    Serial.println("RST OK");
    return;
  }
  const CommandType command = cansat::parse_command(line);
  switch (command) {
    case CommandType::Status:
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "CX");
      Serial.printf(
          "CX OK %s PKT=%lu BATT=%.2fV CURR=%.1fmA SOLAR=%u,%u,%u,%u\n",
          FlightStateMachine::state_name(g_fsm.state()),
          static_cast<unsigned long>(g_packet_count), g_frame.battery_v,
          g_frame.current_ma, g_frame.optical_raw[0], g_frame.optical_raw[1],
          g_frame.optical_raw[2], g_frame.optical_raw[3]);
      break;
    case CommandType::SetTime:
      parse_st_time(line, now_ms);
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "ST");
      Serial.println("ST OK");
      break;
    case CommandType::CalibrateAltitude:
      if (g_baro_last_pa > 0.0f && !g_manual_altitude) {
        g_baro_baseline_pa = g_baro_last_pa;
        g_baro_altitude_m = 0.0f;
      } else {
        g_persistent.altitude_offset_m += g_frame.altitude_m;
      }
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "CAL");
      Serial.println("CAL OK");
      break;
    case CommandType::ActivateMechanism:
      g_mechanism_activated = true;
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "MEC");
      Serial.println("MEC OK (Servo liberado -> 90 deg)");
      break;
    case CommandType::None:
      break;
  }
}

}  // namespace

void setup() {
  Serial.begin(cansat::kSerialBaud);
  pinMode(cansat::kPinLaunchButton, INPUT_PULLUP);
  pinMode(cansat::kPinLedLaunchPad, OUTPUT);
  pinMode(cansat::kPinLedAscent, OUTPUT);
  pinMode(cansat::kPinLedDescent, OUTPUT);
  pinMode(cansat::kPinLedSecondParachute, OUTPUT);
  pinMode(cansat::kPinLedLanded, OUTPUT);
  pinMode(cansat::kPinServoV3, OUTPUT);
  pinMode(cansat::kPinServoMechanism, OUTPUT);

  Wire.begin(cansat::kI2cSdaPin, cansat::kI2cSclPin, cansat::kI2cClockHz);
  g_i2c_bus = &Wire;

  g_mpu_available = init_mpu6050();
  g_baro_available = init_barometer();
  g_fsm.on_event(Event::Arm);
  update_visual_outputs();
  g_last_telemetry_ms = millis();
}

void loop() {
  const uint32_t now = millis();
  const bool btn_pressed = (digitalRead(cansat::kPinLaunchButton) == LOW);
  if (btn_pressed && g_last_button_state) start_auto_flight(now);
  g_last_button_state = !btn_pressed;

  SensorSample sample{};
  read_mpu6050(sample);
  sample.battery_v = 3.70f;
  sample.current_ma = 185.0f;
  sample.optical_raw[0] =
      static_cast<uint16_t>(analogRead(cansat::kPinOptical1));
  sample.optical_raw[1] =
      static_cast<uint16_t>(analogRead(cansat::kPinOptical2));
  sample.optical_raw[2] =
      static_cast<uint16_t>(analogRead(cansat::kPinOptical3));
  sample.optical_raw[3] =
      static_cast<uint16_t>(analogRead(cansat::kPinOptical4));

  float baro_pressure = 0.0f, baro_temperature = 0.0f;
  const bool baro_ok =
      read_barometer(baro_pressure, baro_temperature, g_baro_altitude_m);

  const int pot_raw = analogRead(cansat::kPinAltitudePot);
  if (g_last_pot_raw < 0) {
    g_last_pot_raw = pot_raw;
  } else if (std::abs(pot_raw - g_last_pot_raw) > 80) {
    g_auto_flight_active = false;
    g_manual_altitude = true;
    g_last_pot_raw = pot_raw;
  }

  if (g_auto_flight_active) {
    sample.altitude_m =
        compute_auto_flight_altitude(now - g_auto_flight_start_ms);
  } else if (g_manual_altitude || !baro_ok) {
    sample.altitude_m = (static_cast<float>(pot_raw) / 4095.0f) * 500.0f;
  } else {
    sample.altitude_m = g_baro_altitude_m;
  }
  sample.pressure_pa = pressure_from_altitude(sample.altitude_m);

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
  for (uint8_t i = 0; i < 4; ++i) {
    g_frame.optical_raw[i] = sample.optical_raw[i];
  }

  update_flight_logic(now);
  update_visual_outputs();
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
    std::snprintf(
        g_optional_buffer, sizeof(g_optional_buffer), "MAX:%.1f;P2:%u;CMD:%s",
        g_apogee.max_altitude_m(),
        (g_apogee.second_parachute_triggered() || g_mechanism_activated) ? 1u
                                                                         : 0u,
        g_last_cmd_echo);
    g_frame.team_id = cansat::kTeamId;
    g_frame.mission_time_ms = now;
    g_frame.packet_count = g_packet_count;
    g_frame.mode = g_auto_flight_active ? 1 : 0;
    g_frame.state = g_fsm.state();
    g_frame.optional_data = g_optional_buffer;

    char line[384];
    cansat::format_telemetry_csv(g_frame, line, sizeof(line));
    Serial.println(line);
  }
}
