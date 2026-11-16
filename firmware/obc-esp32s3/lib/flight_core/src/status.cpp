#include "flight_core/status.h"

namespace cansat {
namespace {

// Medio periodo del parpadeo de los LED (500 ms -> 1 Hz).
constexpr uint32_t kBlinkHalfPeriodMs = 500;

bool blink_on(uint32_t now_ms) {
  return ((now_ms / kBlinkHalfPeriodMs) % 2u) == 0u;
}

}  // namespace

SystemSeverity status_severity(const SystemHealth& health) {
  // Sin barometro no hay altitud: es la unica falla que impide cumplir la
  // mision principal (apogeo + segundo paracaidas).
  if (!health.baro_ok) {
    return SystemSeverity::Fault;
  }
  if (!health.imu_ok || !health.ina_ok || !health.fram_ok || !health.rtc_ok ||
      health.rtc_power_loss) {
    return SystemSeverity::Degraded;
  }
  return SystemSeverity::Ok;
}

StatusLeds status_leds(SystemSeverity severity, uint32_t now_ms) {
  StatusLeds leds;
  switch (severity) {
    case SystemSeverity::Ok:
      leds.ok = true;
      leds.fault = false;
      break;
    case SystemSeverity::Degraded:
      leds.ok = blink_on(now_ms);
      leds.fault = false;
      break;
    case SystemSeverity::Fault:
      leds.ok = false;
      leds.fault = blink_on(now_ms);
      break;
  }
  return leds;
}

}  // namespace cansat
