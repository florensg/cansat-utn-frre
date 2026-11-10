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
  uint8_t gps_fix = 0;

  // Magnetometro LIS3MDL (3 ejes en microteslas).
  float mag_x_ut = 0.0f;
  float mag_y_ut = 0.0f;
  float mag_z_ut = 0.0f;

  // Cuatro canales opticos (TSL251RD U4..U7 via ADS1115).
  uint16_t optical_raw[4] = {0, 0, 0, 0};

  // Tiempo UTC sincronizado por GPS NEO-M8N o comando ST.
  uint8_t rtc_hour = 0;
  uint8_t rtc_minute = 0;
  uint8_t rtc_second = 0;
  uint8_t rtc_valid = 0;
};

class ISensorSource {
 public:
  virtual ~ISensorSource() = default;
  virtual bool read(SensorSample& out) = 0;
};

}  // namespace cansat
