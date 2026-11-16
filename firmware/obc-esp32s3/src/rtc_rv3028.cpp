#include "rtc_rv3028.h"

namespace cansat {
namespace {

constexpr uint8_t kRegSeconds = 0x00;
constexpr uint8_t kRegMinutes = 0x01;
constexpr uint8_t kRegHours = 0x02;
constexpr uint8_t kRegWeekday = 0x03;
constexpr uint8_t kRegDate = 0x04;
constexpr uint8_t kRegMonths = 0x05;
constexpr uint8_t kRegYears = 0x06;
constexpr uint8_t kRegStatus = 0x0E;
constexpr uint8_t kRegControl2 = 0x10;

constexpr uint8_t kStatusPorfBit = 0x01;
constexpr uint8_t kControl2HourMode12Bit = 0x02;
constexpr uint8_t kHoursAmPmBit = 0x20;

// Primer registro de hora y cantidad de bytes del bloque.
constexpr uint8_t kTimeBlockFirst = kRegSeconds;
constexpr uint8_t kTimeBlockLength = 7;

bool write_reg(TwoWire& bus, uint8_t address, uint8_t reg, uint8_t value) {
  bus.beginTransmission(address);
  bus.write(reg);
  bus.write(value);
  return bus.endTransmission() == 0;
}

bool read_regs(TwoWire& bus, uint8_t address, uint8_t reg, uint8_t* dest,
               uint8_t len) {
  bus.beginTransmission(address);
  bus.write(reg);
  if (bus.endTransmission() != 0) {
    return false;
  }
  if (bus.requestFrom(static_cast<int>(address), static_cast<int>(len)) != len) {
    return false;
  }
  for (uint8_t i = 0; i < len; ++i) {
    dest[i] = static_cast<uint8_t>(bus.read());
  }
  return true;
}

bool read_reg(TwoWire& bus, uint8_t address, uint8_t reg, uint8_t& out) {
  return read_regs(bus, address, reg, &out, 1);
}

// Si el RTC quedo en modo 12 h el registro de horas trae el bit AM/PM y las
// horas ya no coinciden con UTC: se convierte el valor y se pasa a 24 h.
void force_24h_mode(TwoWire& bus, uint8_t address) {
  uint8_t control2 = 0;
  if (!read_reg(bus, address, kRegControl2, control2)) {
    return;
  }
  if ((control2 & kControl2HourMode12Bit) == 0) {
    return;
  }

  uint8_t raw_hours = 0;
  if (read_reg(bus, address, kRegHours, raw_hours)) {
    const bool pm = (raw_hours & kHoursAmPmBit) != 0;
    uint8_t hours = bcd_to_bin(static_cast<uint8_t>(raw_hours & 0x1F));
    if (pm) {
      hours = static_cast<uint8_t>(hours + 12);
    }
    if (hours == 12 && !pm) {
      hours = 0;  // 12 AM es medianoche
    }
    if (hours == 24) {
      hours = 12;  // 12 PM es mediodia
    }
    if (hours <= 23) {
      write_reg(bus, address, kRegHours, bin_to_bcd(hours));
    }
  }

  write_reg(bus, address, kRegControl2,
            static_cast<uint8_t>(control2 & ~kControl2HourMode12Bit));
}

bool decode_time_block(const uint8_t* raw, DateTime& out) {
  // Orden fisico del bloque: sec, min, hora, weekday, fecha, mes, ano.
  if (!bcd_range_ok(raw[0], 0, 59) || !bcd_range_ok(raw[1], 0, 59) ||
      !bcd_range_ok(raw[2], 0, 23) || !bcd_range_ok(raw[3], 1, 7) ||
      !bcd_range_ok(raw[4], 1, 31) || !bcd_range_ok(raw[5], 1, 12) ||
      !bcd_range_ok(raw[6], 0, 99)) {
    return false;
  }

  out.second = bcd_to_bin(raw[0]);
  out.minute = bcd_to_bin(raw[1]);
  out.hour = bcd_to_bin(static_cast<uint8_t>(raw[2] & 0x3F));
  out.day = bcd_to_bin(raw[4]);
  out.month = bcd_to_bin(static_cast<uint8_t>(raw[5] & 0x1F));
  out.year = static_cast<uint16_t>(2000u + bcd_to_bin(raw[6]));
  return datetime_is_valid(out);
}

}  // namespace

RtcHealth rtc_init(TwoWire& bus, uint8_t address) {
  RtcHealth health;

  uint8_t probe[kTimeBlockLength] = {0};
  if (!read_regs(bus, address, kTimeBlockFirst, probe, kTimeBlockLength)) {
    return health;  // no responde: no hay RTC en el bus
  }
  health.present = true;

  force_24h_mode(bus, address);

  uint8_t status = 0;
  if (read_reg(bus, address, kRegStatus, status)) {
    health.status = status;
    health.power_loss = (status & kStatusPorfBit) != 0;
  }

  DateTime dt;
  const bool decoded = rtc_read_datetime(bus, address, dt);
  // Con PORF activo la hora existe pero no es confiable: se informa igual para
  // que la GCS vea el valor, pero no se usa como fuente.
  health.time_valid = decoded && !health.power_loss;
  return health;
}

bool rtc_read_datetime(TwoWire& bus, uint8_t address, DateTime& out) {
  uint8_t raw[kTimeBlockLength] = {0};
  if (!read_regs(bus, address, kTimeBlockFirst, raw, kTimeBlockLength)) {
    return false;
  }
  return decode_time_block(raw, out);
}

bool rtc_write_datetime(TwoWire& bus, uint8_t address, const DateTime& dt) {
  if (!datetime_is_valid(dt)) {
    return false;
  }

  uint8_t raw[kTimeBlockLength];
  raw[0] = bin_to_bcd(dt.second);
  raw[1] = bin_to_bcd(dt.minute);
  raw[2] = bin_to_bcd(dt.hour);
  raw[3] = bin_to_bcd(weekday_iso(dt));
  raw[4] = bin_to_bcd(dt.day);
  raw[5] = bin_to_bcd(dt.month);
  raw[6] = bin_to_bcd(static_cast<uint8_t>(dt.year % 100u));

  bus.beginTransmission(address);
  bus.write(kTimeBlockFirst);
  for (uint8_t i = 0; i < kTimeBlockLength; ++i) {
    bus.write(raw[i]);
  }
  if (bus.endTransmission() != 0) {
    return false;
  }

  rtc_clear_flags(bus, address);
  return true;
}

bool rtc_clear_flags(TwoWire& bus, uint8_t address) {
  return write_reg(bus, address, kRegStatus, 0x00);
}

}  // namespace cansat
