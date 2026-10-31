#pragma once

#include <cstdint>

namespace cansat {

struct SensorSample {
  uint32_t timestamp_ms = 0;
  float altitude_m = 0.0f;
  float pressure_pa = 0.0f;
  float temperature_c = 0.0f;
  float battery_v = 0.0f;
  float current_ma = 0.0f;
  float accel_x_g = 0.0f;
  float accel_y_g = 0.0f;
  float accel_z_g = 0.0f;
  float gyro_x_dps = 0.0f;
  float gyro_y_dps = 0.0f;
  float gyro_z_dps = 0.0f;
  float latitude_deg = 0.0f;
  float longitude_deg = 0.0f;
  float gps_altitude_m = 0.0f;
  uint8_t satellites = 0;
};

class ISensorSource {
 public:
  virtual ~ISensorSource() = default;
  virtual bool read(SensorSample& out) = 0;
};

class SimulatedSensorSource final : public ISensorSource {
 public:
  bool read(SensorSample& out) override;

 private:
  uint32_t tick_ = 0;
};

}  // namespace cansat
