#pragma once

#include <cstdint>

namespace cansat {

// ---------------------------------------------------------------------------
// Salud del OBC resumida para el LED de estado (requisito E6).
//
// Se separa "critico" de "degradado":
//   - barometro: critico. Sin altitud no hay deteccion de apogeo ni de segundo
//     paracaidas; el vuelo no cumple su funcion.
//   - IMU / INA219 / FRAM / RTC: degradados. El vuelo sigue, pero se pierde
//     actitud, medicion de bateria o persistencia del tiempo de mision (F2).
// ---------------------------------------------------------------------------
struct SystemHealth {
  bool imu_ok = false;
  bool baro_ok = false;
  bool ina_ok = false;
  bool fram_ok = false;
  bool rtc_ok = false;
  bool rtc_power_loss = false;
};

enum class SystemSeverity : uint8_t {
  Ok = 0,
  Degraded = 1,
  Fault = 2,
};

// Fault si falta el barometro; Degraded si falta cualquier otro subsistema;
// Ok si estan todos.
SystemSeverity status_severity(const SystemHealth& health);

struct StatusLeds {
  bool ok = false;
  bool fault = false;
};

// Patrones (periodo completo 1 s, medio periodo 500 ms):
//   Ok        -> LED OK fijo.
//   Degraded  -> LED OK parpadea.
//   Fault     -> LED OK apagado, LED de falla parpadea.
// El parpadeo tambien sirve de latido: si el firmware se cuelga, el patron se
// congela y se nota a simple vista.
StatusLeds status_leds(SystemSeverity severity, uint32_t now_ms);

}  // namespace cansat
