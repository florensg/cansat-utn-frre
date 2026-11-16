#include "power_manager.h"

#include <Arduino.h>

#include "config.h"
#include "flight_core/power.h"

namespace cansat {
namespace {

PowerMask g_mask = 0;
bool g_payload_prepared = false;

uint8_t pin_for_rail(PowerRail rail) {
  switch (rail) {
    case PowerRail::Rtc:
      return kPowerPinRtc;
    case PowerRail::Fram:
      return kPowerPinFram;
    case PowerRail::Sensors:
      return kPowerPinSensors;
    case PowerRail::Gps:
      return kPowerPinGps;
    case PowerRail::Radio:
      return kPowerPinRadio;
    case PowerRail::Rpi:
      return kPowerPinRpi;
    case PowerRail::XiaoCenit:
      return kPowerPinXiaoCenit;
    case PowerRail::XiaoNadir:
      return kPowerPinXiaoNadir;
    case PowerRail::Servo:
      return kPowerPinServo;
    case PowerRail::Sec:
      return kPowerPinSec;
    case PowerRail::Count:
      break;
  }
  return kPowerPinUnassigned;
}

// Activa/desactiva el riel. La compuerta del 2N7002 es activa en HIGH.
void write_rail(PowerRail rail, bool on) {
  const uint8_t pin = pin_for_rail(rail);
  if (pin == kPowerPinUnassigned) {
    return;
  }
  digitalWrite(pin, on ? HIGH : LOW);
}

void drive_all_off() {
  for (uint8_t i = 0; i < static_cast<uint8_t>(PowerRail::Count); ++i) {
    write_rail(static_cast<PowerRail>(i), false);
  }
}

}  // namespace

void power_manager_init() {
  if (!kPowerGatingEnabled) {
    // Sin gating: no se toca ningun pin y todos los rieles cuentan como ON.
    g_mask = kPowerAll;
    return;
  }

  for (uint8_t i = 0; i < static_cast<uint8_t>(PowerRail::Count); ++i) {
    const uint8_t pin = pin_for_rail(static_cast<PowerRail>(i));
    if (pin != kPowerPinUnassigned) {
      pinMode(pin, OUTPUT);
    }
  }
  drive_all_off();
  g_mask = 0;
  power_manager_apply(State::LaunchPad, false);
}

PowerMask power_manager_apply(State state, bool payload_prepared) {
  if (!kPowerGatingEnabled) {
    g_mask = kPowerAll;
    return 0;
  }

  g_payload_prepared = payload_prepared;
  const PowerMask target = power_rails_for_state(state, payload_prepared);
  const PowerMask changed = static_cast<PowerMask>(target ^ g_mask);

  if (changed != 0) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(PowerRail::Count); ++i) {
      const PowerRail rail = static_cast<PowerRail>(i);
      if (power_mask_has(changed, rail)) {
        write_rail(rail, power_mask_has(target, rail));
      }
    }
    delay(kPowerRailSettleMs);
  }

  g_mask = target;
  return changed;
}

PowerMask power_manager_mask() { return g_mask; }

bool power_manager_rail_on(PowerRail rail) { return power_mask_has(g_mask, rail); }

void power_manager_set_payload_prepared(bool on) { g_payload_prepared = on; }

bool power_manager_payload_prepared() { return g_payload_prepared; }

}  // namespace cansat
