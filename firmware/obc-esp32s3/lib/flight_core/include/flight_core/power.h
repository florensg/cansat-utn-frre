#pragma once

#include <cstdint>

#include "flight_core/fsm.h"

namespace cansat {

// Rieles conmutables del circuito v.2 (TPS2553 / TPS22946 + AO3401A/2N7002).
enum class PowerRail : uint8_t {
  Rtc = 0,
  Fram = 1,
  Sensors = 2,
  Gps = 3,
  Radio = 4,
  Rpi = 5,
  XiaoCenit = 6,
  XiaoNadir = 7,
  Servo = 8,
  Sec = 9,
  Count = 10,
};

using PowerMask = uint16_t;

constexpr PowerMask power_bit(PowerRail rail) {
  return static_cast<PowerMask>(1u << static_cast<uint8_t>(rail));
}

constexpr PowerMask kPowerAll =
    static_cast<PowerMask>((1u << static_cast<uint8_t>(PowerRail::Count)) - 1u);

constexpr bool power_mask_has(PowerMask mask, PowerRail rail) {
  return (mask & power_bit(rail)) != 0;
}

// Rieles que deben estar encendidos en cada estado de vuelo.
// payload_prepared: override manual de la GCS (comando RPI) para preparar la
// carga util (RPi + camaras XIAO + servo) antes del lanzamiento.
PowerMask power_rails_for_state(State state, bool payload_prepared);

const char* power_rail_name(PowerRail rail);

}  // namespace cansat
