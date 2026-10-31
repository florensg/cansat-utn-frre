#include "flight_core/flight.h"

namespace cansat {

ApogeeDetector::ApogeeDetector(const ApogeeConfig& config) : config_(config) {}

FlightEvent ApogeeDetector::update(float altitude_m, uint32_t timestamp_ms) {
  if (timestamp_ms < config_.min_flight_time_ms) {
    return FlightEvent::None;
  }

  if (altitude_m > max_altitude_m_) {
    max_altitude_m_ = altitude_m;
    max_altitude_time_ms_ = timestamp_ms;
    return FlightEvent::None;
  }

  if (max_altitude_m_ < config_.min_altitude_m) {
    return FlightEvent::None;
  }

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

void ApogeeDetector::reset() {
  max_altitude_m_ = 0.0f;
  max_altitude_time_ms_ = 0;
  apogee_detected_ = false;
  second_parachute_triggered_ = false;
}

}  // namespace cansat
