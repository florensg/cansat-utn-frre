#include "flight_core/sensors.h"

namespace cansat {

bool SimulatedSensorSource::read(SensorSample& out) {
  out = SensorSample{};
  out.timestamp_ms = tick_ * 1000u;
  ++tick_;
  return true;
}

}  // namespace cansat
