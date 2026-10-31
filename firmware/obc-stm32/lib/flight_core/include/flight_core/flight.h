#pragma once

#include <cstdint>

namespace cansat {

enum class FlightEvent : uint8_t {
  None = 0,
  ApogeeDetected = 1,
  SecondParachuteTriggered = 2,
};

struct ApogeeConfig {
  float min_altitude_m = 100.0f;
  float apogee_drop_m = 5.0f;
  float second_parachute_ratio = 0.80f;
  uint32_t min_flight_time_ms = 10000;
};

class ApogeeDetector {
 public:
  explicit ApogeeDetector(const ApogeeConfig& config = ApogeeConfig{});

  FlightEvent update(float altitude_m, uint32_t timestamp_ms);
  void reset();

  float max_altitude_m() const { return max_altitude_m_; }
  bool apogee_detected() const { return apogee_detected_; }
  bool second_parachute_triggered() const {
    return second_parachute_triggered_;
  }

 private:
  ApogeeConfig config_;
  float max_altitude_m_ = 0.0f;
  uint32_t max_altitude_time_ms_ = 0;
  bool apogee_detected_ = false;
  bool second_parachute_triggered_ = false;
};

}  // namespace cansat
