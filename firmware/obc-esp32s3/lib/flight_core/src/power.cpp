#include "flight_core/power.h"

namespace cansat {
namespace {

// Rieles que deben estar siempre disponibles para la mision principal:
// reloj, memoria persistente, sensores de vuelo, GPS y radio.
constexpr PowerMask kBase = power_bit(PowerRail::Rtc) |
                            power_bit(PowerRail::Fram) |
                            power_bit(PowerRail::Sensors) |
                            power_bit(PowerRail::Gps) |
                            power_bit(PowerRail::Radio);

// Carga util que se prepara antes del lanzamiento (comando RPI):
// Raspberry Pi Zero + las dos camaras XIAO + servo del segundo paracaidas.
constexpr PowerMask kPayload = power_bit(PowerRail::Rpi) |
                               power_bit(PowerRail::XiaoCenit) |
                               power_bit(PowerRail::XiaoNadir) |
                               power_bit(PowerRail::Servo) |
                               power_bit(PowerRail::Sec);

// En vuelo todo debe estar encendido.
constexpr PowerMask kFlight = kBase | kPayload;

}  // namespace

PowerMask power_rails_for_state(State state, bool payload_prepared) {
  PowerMask mask = kBase;
  switch (state) {
    case State::LaunchPad:
      // RPi, camaras y servo apagados hasta el comando RPI (T-60 s).
      break;
    case State::Ascent:
    case State::Descent:
      mask = kFlight;
      break;
    case State::Landed:
      // Se apagan RPi, camaras y servo para ahorrar bateria en recuperacion.
      break;
  }

  // El override de la GCS solo aplica antes del aterrizaje. En Landed se
  // ignora para priorizar la autonomia de la baliza y la radio.
  if (payload_prepared && state != State::Landed) {
    mask |= kPayload;
  }
  return mask;
}

const char* power_rail_name(PowerRail rail) {
  switch (rail) {
    case PowerRail::Rtc:
      return "RTC";
    case PowerRail::Fram:
      return "FRAM";
    case PowerRail::Sensors:
      return "SENS";
    case PowerRail::Gps:
      return "GPS";
    case PowerRail::Radio:
      return "RADIO";
    case PowerRail::Rpi:
      return "RPI";
    case PowerRail::XiaoCenit:
      return "XIAO_C";
    case PowerRail::XiaoNadir:
      return "XIAO_N";
    case PowerRail::Servo:
      return "SERVO";
    case PowerRail::Sec:
      return "SEC";
    case PowerRail::Count:
      break;
  }
  return "?";
}

}  // namespace cansat
