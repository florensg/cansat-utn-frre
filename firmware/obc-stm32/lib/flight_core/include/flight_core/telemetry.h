#pragma once

#include <cstddef>
#include <cstdint>

#include "flight_core/fsm.h"

namespace cansat {

struct TelemetryFrame {
  uint16_t team_id = 0;
  uint32_t mission_time_ms = 0;
  uint32_t packet_count = 0;
  uint8_t mode = 0;
  State state = State::LaunchPad;
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
  const char* optional_data = "";
};

std::size_t format_telemetry_csv(const TelemetryFrame& frame, char* buffer,
                                 std::size_t buffer_size);

uint32_t next_packet_count(uint32_t current);

}  // namespace cansat
