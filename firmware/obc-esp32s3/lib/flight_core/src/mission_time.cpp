#include "flight_core/mission_time.h"

namespace cansat {
namespace {

constexpr uint32_t kSecondsPerDay = 86400u;
constexpr uint32_t kSecondsPerHour = 3600u;
constexpr uint32_t kSecondsPerMinute = 60u;

// Rango de anos que el RV-3028-C7 puede representar (registro de 2 digitos).
constexpr uint16_t kMinYear = 2000;
constexpr uint16_t kMaxYear = 2099;

}  // namespace

bool is_leap_year(uint16_t year) {
  if (year % 4 != 0) {
    return false;
  }
  if (year % 100 != 0) {
    return true;
  }
  return year % 400 == 0;
}

uint8_t days_in_month(uint16_t year, uint8_t month) {
  switch (month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      return 31;
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
    case 2:
      return is_leap_year(year) ? 29 : 28;
    default:
      return 0;
  }
}

bool datetime_is_valid(const DateTime& dt) {
  if (dt.year < kMinYear || dt.year > kMaxYear) {
    return false;
  }
  if (dt.month < 1 || dt.month > 12) {
    return false;
  }
  if (dt.day < 1 || dt.day > days_in_month(dt.year, dt.month)) {
    return false;
  }
  if (dt.hour > 23 || dt.minute > 59 || dt.second > 59) {
    return false;
  }
  return true;
}

uint8_t weekday_iso(const DateTime& dt) {
  // 2000-01-01 fue sabado (ISO 6). Solo se usa el campo de fecha.
  uint32_t days = 0;
  for (uint16_t y = kMinYear; y < dt.year && y <= kMaxYear; ++y) {
    days += is_leap_year(y) ? 366u : 365u;
  }
  for (uint8_t m = 1; m < dt.month && m <= 12; ++m) {
    days += days_in_month(dt.year, m);
  }
  if (dt.day > 0) {
    days += static_cast<uint32_t>(dt.day - 1);
  }
  return static_cast<uint8_t>(((days + 5u) % 7u) + 1u);
}

uint32_t datetime_to_seconds(const DateTime& dt) {
  uint32_t days = 0;
  for (uint16_t y = kMinYear; y < dt.year && y <= kMaxYear; ++y) {
    days += is_leap_year(y) ? 366u : 365u;
  }
  for (uint8_t m = 1; m < dt.month && m <= 12; ++m) {
    days += days_in_month(dt.year, m);
  }
  if (dt.day > 0) {
    days += static_cast<uint32_t>(dt.day - 1);
  }
  return days * kSecondsPerDay +
         static_cast<uint32_t>(dt.hour) * kSecondsPerHour +
         static_cast<uint32_t>(dt.minute) * kSecondsPerMinute + dt.second;
}

DateTime seconds_to_datetime(uint32_t seconds) {
  DateTime dt;
  uint32_t days = seconds / kSecondsPerDay;
  uint32_t rest = seconds % kSecondsPerDay;

  dt.hour = static_cast<uint8_t>(rest / kSecondsPerHour);
  rest %= kSecondsPerHour;
  dt.minute = static_cast<uint8_t>(rest / kSecondsPerMinute);
  dt.second = static_cast<uint8_t>(rest % kSecondsPerMinute);

  uint16_t year = kMinYear;
  while (year < kMaxYear) {
    const uint32_t year_days = is_leap_year(year) ? 366u : 365u;
    if (days < year_days) {
      break;
    }
    days -= year_days;
    ++year;
  }
  dt.year = year;

  uint8_t month = 1;
  while (month < 12) {
    const uint8_t month_days = days_in_month(year, month);
    if (days < month_days) {
      break;
    }
    days -= month_days;
    ++month;
  }
  dt.month = month;
  dt.day = static_cast<uint8_t>(days + 1u);
  return dt;
}

uint8_t bcd_to_bin(uint8_t bcd) {
  return static_cast<uint8_t>(((bcd >> 4) * 10u) + (bcd & 0x0Fu));
}

uint8_t bin_to_bcd(uint8_t bin) {
  return static_cast<uint8_t>(((bin / 10u) << 4) | (bin % 10u));
}

bool bcd_range_ok(uint8_t bcd, uint8_t min_value, uint8_t max_value) {
  const uint8_t high = static_cast<uint8_t>(bcd >> 4);
  const uint8_t low = static_cast<uint8_t>(bcd & 0x0Fu);
  if (high > 9 || low > 9) {
    return false;
  }
  const uint8_t value = bcd_to_bin(bcd);
  return value >= min_value && value <= max_value;
}

bool time_source_accepts(TimeSource current, TimeSource candidate,
                         bool command_hold_active) {
  // millis() nunca sobrescribe una fuente real; solo puede ser el estado inicial.
  if (candidate == TimeSource::None || candidate == TimeSource::Millis) {
    return false;
  }
  // La correccion manual desde tierra siempre se aplica.
  if (candidate == TimeSource::Command) {
    return true;
  }
  if (current == TimeSource::None || current == TimeSource::Millis) {
    return true;
  }
  if (current == TimeSource::Command) {
    // Mientras la ventana de gracia este vigente, el GPS no pisa la correccion
    // manual (evita que un ajuste fino se revierta por el desfasaje del receptor).
    // Vencida la ventana, el GPS retoma el control; el RTC nunca la reemplaza.
    return candidate == TimeSource::Gps && !command_hold_active;
  }
  // Jerarquia restante: GPS (3) > RTC (2) > millis (1).
  return static_cast<uint8_t>(candidate) >= static_cast<uint8_t>(current);
}

const char* time_source_name(TimeSource source) {
  switch (source) {
    case TimeSource::None:
      return "NONE";
    case TimeSource::Millis:
      return "MILLIS";
    case TimeSource::Rtc:
      return "RTC";
    case TimeSource::Gps:
      return "GPS";
    case TimeSource::Command:
      return "CMD";
  }
  return "NONE";
}

char time_source_tag(TimeSource source) {
  switch (source) {
    case TimeSource::Gps:
      return 'G';
    case TimeSource::Command:
      return 'C';
    case TimeSource::Rtc:
      return 'R';
    case TimeSource::Millis:
      return 'M';
    case TimeSource::None:
      break;
  }
  return '-';
}

bool MissionClock::synced() const {
  return source_ != TimeSource::None && source_ != TimeSource::Millis;
}

bool MissionClock::command_hold_active(uint32_t now_ms) const {
  return source_ == TimeSource::Command &&
         static_cast<int32_t>(command_hold_until_ms_ - now_ms) > 0;
}

bool MissionClock::accepts(TimeSource candidate, uint32_t now_ms) const {
  return time_source_accepts(source_, candidate, command_hold_active(now_ms));
}

void MissionClock::sync(TimeSource source, const DateTime& dt,
                        uint32_t now_ms) {
  if (!datetime_is_valid(dt) || !accepts(source, now_ms)) {
    return;
  }
  base_seconds_ = datetime_to_seconds(dt);
  base_millis_ = now_ms;
  source_ = source;
  last_datetime_ = dt;
  if (source == TimeSource::Command) {
    command_hold_until_ms_ = now_ms + kCommandTimeHoldMs;
  }
}

void MissionClock::sync_time_only(TimeSource source, uint8_t hour,
                                  uint8_t minute, uint8_t second,
                                  uint32_t now_ms) {
  if (hour > 23 || minute > 59 || second > 59) {
    return;
  }
  if (!accepts(source, now_ms)) {
    return;
  }

  DateTime dt = seconds_to_datetime(seconds_since_epoch(now_ms));
  if (!synced()) {
    // Sin fecha conocida no se puede inventar el dia: se usa el de referencia.
    dt.year = 2000;
    dt.month = 1;
    dt.day = 1;
  }
  dt.hour = hour;
  dt.minute = minute;
  dt.second = second;

  base_seconds_ = datetime_to_seconds(dt);
  base_millis_ = now_ms;
  source_ = source;
  last_datetime_ = dt;
  if (source == TimeSource::Command) {
    command_hold_until_ms_ = now_ms + kCommandTimeHoldMs;
  }
}

uint32_t MissionClock::seconds_since_epoch(uint32_t now_ms) const {
  const uint32_t elapsed_s = (now_ms - base_millis_) / 1000u;
  return base_seconds_ + elapsed_s;
}

DateTime MissionClock::now(uint32_t now_ms) const {
  return seconds_to_datetime(seconds_since_epoch(now_ms));
}

void MissionClock::get_hms(uint32_t now_ms, uint8_t& hour, uint8_t& minute,
                           uint8_t& second, uint8_t& valid) const {
  const DateTime dt = now(now_ms);
  hour = dt.hour;
  minute = dt.minute;
  second = dt.second;
  valid = synced() ? 1u : 0u;
}

void MissionClock::reset() {
  base_seconds_ = 0;
  base_millis_ = 0;
  command_hold_until_ms_ = 0;
  source_ = TimeSource::None;
  last_datetime_ = DateTime{};
}

}  // namespace cansat
