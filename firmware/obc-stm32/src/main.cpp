#include <Arduino.h>

#include <cstddef>

#include "config.h"
#include "flight_core/commands.h"
#include "flight_core/flight.h"
#include "flight_core/fsm.h"
#include "flight_core/sensors.h"
#include "flight_core/storage.h"
#include "flight_core/telemetry.h"

namespace {

using cansat::ApogeeDetector;
using cansat::CommandType;
using cansat::FlightStateMachine;
using cansat::MemoryStorage;
using cansat::PersistentState;
using cansat::SensorSample;
using cansat::SimulatedSensorSource;
using cansat::TelemetryFrame;

FlightStateMachine g_fsm;
ApogeeDetector g_apogee({cansat::kMinApogeeAltitudeM, cansat::kApogeeDropM,
                         cansat::kSecondParachuteRatio,
                         cansat::kMinFlightTimeMs});
MemoryStorage g_storage;
SimulatedSensorSource g_sensors;
TelemetryFrame g_frame{};
PersistentState g_persistent{};
uint32_t g_packet_count = 0;
uint32_t g_last_telemetry_ms = 0;

void apply_sample(const SensorSample& sample) {
  g_frame.altitude_m = sample.altitude_m;
  g_frame.pressure_pa = sample.pressure_pa;
  g_frame.temperature_c = sample.temperature_c;
  g_frame.battery_v = sample.battery_v;
  g_frame.current_ma = sample.current_ma;
  g_frame.accel_x_g = sample.accel_x_g;
  g_frame.accel_y_g = sample.accel_y_g;
  g_frame.accel_z_g = sample.accel_z_g;
  g_frame.gyro_x_dps = sample.gyro_x_dps;
  g_frame.gyro_y_dps = sample.gyro_y_dps;
  g_frame.gyro_z_dps = sample.gyro_z_dps;
  g_frame.latitude_deg = sample.latitude_deg;
  g_frame.longitude_deg = sample.longitude_deg;
  g_frame.gps_altitude_m = sample.gps_altitude_m;
  g_frame.satellites = sample.satellites;
}

void persist_state(uint32_t mission_time_ms) {
  g_persistent.magic = cansat::kPersistentMagic;
  g_persistent.version = cansat::kPersistentVersion;
  g_persistent.packet_count = g_packet_count;
  g_persistent.mission_time_ms = mission_time_ms;
  g_persistent.state = g_fsm.state();
  g_persistent.armed = g_fsm.armed();
  g_persistent.second_parachute_deployed = g_apogee.second_parachute_triggered();
  g_storage.save(g_persistent);
}

void handle_command(const char* line) {
  const CommandType command = cansat::parse_command(line);
  switch (command) {
    case CommandType::Status:
      Serial.print("CX OK ");
      Serial.println(g_packet_count);
      break;
    case CommandType::SetTime:
      Serial.println("ST OK");
      break;
    case CommandType::CalibrateAltitude:
      g_persistent.altitude_offset_m = g_frame.altitude_m;
      persist_state(g_frame.mission_time_ms);
      Serial.println("CAL OK");
      break;
    case CommandType::ActivateMechanism:
      Serial.println("MEC OK");
      break;
    case CommandType::None:
      break;
  }
}

}  // namespace

void setup() {
  Serial.begin(cansat::kSerialBaud);

  PersistentState restored{};
  if (g_storage.load(restored)) {
    g_packet_count = restored.packet_count;
    g_fsm.restore(restored.state, restored.armed);
    g_persistent = restored;
  }

  g_last_telemetry_ms = millis();
}

void loop() {
  const uint32_t now = millis();

  SensorSample sample{};
  if (g_sensors.read(sample)) {
    apply_sample(sample);
  }

  if (Serial.available() > 0) {
    char command_line[64];
    const std::size_t length =
        Serial.readBytesUntil('\n', command_line, sizeof(command_line) - 1);
    command_line[length] = '\0';
    handle_command(command_line);
  }

  if ((now - g_last_telemetry_ms) >= cansat::kTelemetryPeriodMs) {
    g_last_telemetry_ms += cansat::kTelemetryPeriodMs;
    g_packet_count = cansat::next_packet_count(g_packet_count);

    g_frame.team_id = cansat::kTeamId;
    g_frame.mission_time_ms = now;
    g_frame.packet_count = g_packet_count;
    g_frame.state = g_fsm.state();

    char line[256];
    cansat::format_telemetry_csv(g_frame, line, sizeof(line));
    Serial.println(line);

    persist_state(now);
  }
}
