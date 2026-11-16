#pragma once

#include <cstdint>

namespace cansat {

// ---------------------------------------------------------------------------
// Fecha/hora civil en UTC. No hay zona horaria ni horario de verano: el CanSat
// vuela siempre en UTC, igual que el GPS y las planillas de la competencia.
// ---------------------------------------------------------------------------
struct DateTime {
  uint16_t year = 2000;
  uint8_t month = 1;
  uint8_t day = 1;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

// Origen del tiempo. La jerarquia operativa es GPS > RTC > millis(); el comando
// ST desde tierra es una correccion manual explicita y se ubica por encima de
// ambos, con una ventana de gracia para que el GPS no la pise al instante.
enum class TimeSource : uint8_t {
  None = 0,
  Millis = 1,
  Rtc = 2,
  Gps = 3,
  Command = 4,
};

// Cuantos ms se respeta una correccion manual antes de que el GPS retome el
// control. Cubre el margen de 1 s que exige el requisito F3 con holgura.
constexpr uint32_t kCommandTimeHoldMs = 5000;

// ---------------------------------------------------------------------------
// Calendario (puro, sin dependencias del hardware: testeable en `native`).
// ---------------------------------------------------------------------------
bool is_leap_year(uint16_t year);
uint8_t days_in_month(uint16_t year, uint8_t month);
bool datetime_is_valid(const DateTime& dt);

// 1 = lunes ... 7 = domingo (ISO-8601).
uint8_t weekday_iso(const DateTime& dt);

// Segundos desde 2000-01-01T00:00:00Z. Rango util: 2000..2099.
uint32_t datetime_to_seconds(const DateTime& dt);
DateTime seconds_to_datetime(uint32_t seconds);

// ---------------------------------------------------------------------------
// BCD: el RV-3028-C7 guarda las horas en binario codificado decimal.
// ---------------------------------------------------------------------------
uint8_t bcd_to_bin(uint8_t bcd);
uint8_t bin_to_bcd(uint8_t bin);
// true si ambos nibbles son digitos validos y el valor cae en [min, max].
bool bcd_range_ok(uint8_t bcd, uint8_t min_value, uint8_t max_value);

// ---------------------------------------------------------------------------
// Resolucion de precedencia entre fuentes de tiempo.
// `command_hold_active` indica que una correccion manual sigue vigente.
// ---------------------------------------------------------------------------
bool time_source_accepts(TimeSource current, TimeSource candidate,
                         bool command_hold_active);
const char* time_source_name(TimeSource source);
// 'G' GPS, 'C' comando, 'R' RTC, 'M' millis, '-' sin sincronizar.
char time_source_tag(TimeSource source);

// ---------------------------------------------------------------------------
// Reloj de mision: guarda un ancla (segundos + millis de boot) y avanza con
// millis(). La fuente solo decide quien puede mover el ancla.
// ---------------------------------------------------------------------------
class MissionClock {
 public:
  void sync(TimeSource source, const DateTime& dt, uint32_t now_ms);
  // Sincroniza solo la hora del dia. Si el reloj nunca tuvo fecha, usa
  // 2000-01-01 como fecha provisoria.
  void sync_time_only(TimeSource source, uint8_t hour, uint8_t minute,
                      uint8_t second, uint32_t now_ms);

  DateTime now(uint32_t now_ms) const;
  uint32_t seconds_since_epoch(uint32_t now_ms) const;

  // Compatibilidad con la telemetria: h/m/s + flag de validez.
  void get_hms(uint32_t now_ms, uint8_t& hour, uint8_t& minute, uint8_t& second,
               uint8_t& valid) const;

  TimeSource source() const { return source_; }
  bool synced() const;
  bool command_hold_active(uint32_t now_ms) const;
  const DateTime& last_sync_datetime() const { return last_datetime_; }

  void reset();

 private:
  bool accepts(TimeSource candidate, uint32_t now_ms) const;

  uint32_t base_seconds_ = 0;
  uint32_t base_millis_ = 0;
  uint32_t command_hold_until_ms_ = 0;
  TimeSource source_ = TimeSource::None;
  DateTime last_datetime_{};
};

}  // namespace cansat
