#include "flight_core/telemetry.h"

#include <cstdio>

namespace cansat {

std::size_t format_telemetry_csv(const TelemetryFrame& frame, char* buffer,
                                 std::size_t buffer_size) {
  if (buffer == nullptr || buffer_size == 0) {
    return 0;
  }

  const int written = std::snprintf(
      buffer, buffer_size,
      "%u,%lu,%lu,%u,%s,%.2f,%.2f,%.2f,%.2f,%.2f,"
      "%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.6f,%.6f,%.2f,%u,%s",
      static_cast<unsigned>(frame.team_id),
      static_cast<unsigned long>(frame.mission_time_ms),
      static_cast<unsigned long>(frame.packet_count),
      static_cast<unsigned>(frame.mode),
      FlightStateMachine::state_name(frame.state), frame.altitude_m,
      frame.pressure_pa, frame.temperature_c, frame.battery_v, frame.current_ma,
      frame.accel_x_g, frame.accel_y_g, frame.accel_z_g, frame.gyro_x_dps,
      frame.gyro_y_dps, frame.gyro_z_dps, frame.latitude_deg,
      frame.longitude_deg, frame.gps_altitude_m,
      static_cast<unsigned>(frame.satellites),
      frame.optional_data == nullptr ? "" : frame.optional_data);

  if (written < 0) {
    buffer[0] = '\0';
    return 0;
  }

  const std::size_t length = static_cast<std::size_t>(written);
  return length < buffer_size ? length : buffer_size - 1;
}

uint32_t next_packet_count(uint32_t current) { return current + 1; }

}  // namespace cansat
