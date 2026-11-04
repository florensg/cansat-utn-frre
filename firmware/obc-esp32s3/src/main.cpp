#include <Arduino.h>
#include <Wire.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

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
using cansat::Event;
using cansat::FlightEvent;
using cansat::FlightStateMachine;
using cansat::MemoryStorage;
using cansat::PersistentState;
using cansat::SensorSample;
using cansat::SimulatedSensorSource;
using cansat::State;
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
uint32_t g_last_servo_pulse_ms = 0;
uint32_t g_launch_start_ms = 0;

bool g_mpu_available = false;
bool g_auto_flight_active = false;
uint32_t g_auto_flight_start_ms = 0;
bool g_mechanism_activated = false;
bool g_last_button_state = true;
int g_last_pot_raw = -1;
char g_last_cmd_echo[16] = "NONE";
char g_optional_buffer[64] = "";

bool init_mpu6050() {
  Wire.beginTransmission(cansat::kMpu6050I2cAddress);
  Wire.write(0x6B);  // PWR_MGMT_1
  Wire.write(0x00);  // Wake up MPU-6050
  return Wire.endTransmission() == 0;
}

bool read_mpu6050(SensorSample& sample) {
  if (!g_mpu_available) {
    return false;
  }
  Wire.beginTransmission(cansat::kMpu6050I2cAddress);
  Wire.write(0x3B);  // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  const uint8_t bytes_read =
      Wire.requestFrom(cansat::kMpu6050I2cAddress, static_cast<uint8_t>(14));
  if (bytes_read < 14) {
    return false;
  }

  auto read_i16 = []() -> int16_t {
    const uint8_t hi = Wire.read();
    const uint8_t lo = Wire.read();
    return static_cast<int16_t>((static_cast<uint16_t>(hi) << 8) | lo);
  };

  const int16_t ax = read_i16();
  const int16_t ay = read_i16();
  const int16_t az = read_i16();
  const int16_t temp_raw = read_i16();
  const int16_t gx = read_i16();
  const int16_t gy = read_i16();
  const int16_t gz = read_i16();

  sample.accel_x_g = static_cast<float>(ax) / 16384.0f;
  sample.accel_y_g = static_cast<float>(ay) / 16384.0f;
  sample.accel_z_g = static_cast<float>(az) / 16384.0f;
  sample.temperature_c = (static_cast<float>(temp_raw) / 340.0f) + 36.53f;
  sample.gyro_x_dps = static_cast<float>(gx) / 131.0f;
  sample.gyro_y_dps = static_cast<float>(gy) / 131.0f;
  sample.gyro_z_dps = static_cast<float>(gz) / 131.0f;
  return true;
}

float compute_auto_flight_altitude(uint32_t elapsed_ms) {
  // Perfil sintetico de vuelo (duracion ~35 s):
  // 0 - 15 s: Ascenso hasta 400 m
  // 15 - 32 s: Descenso desde 400 m hasta 0 m (pasa por apogeo ~395 m y 80% a 320 m)
  // > 32 s: Aterrizado en 0 m
  const float t_sec = static_cast<float>(elapsed_ms) / 1000.0f;
  if (t_sec <= 15.0f) {
    return (t_sec / 15.0f) * 400.0f;
  }
  if (t_sec <= 32.0f) {
    const float descent_ratio = (t_sec - 15.0f) / 17.0f;
    const float alt = 400.0f * (1.0f - descent_ratio);
    return alt > 0.0f ? alt : 0.0f;
  }
  return 0.0f;
}

float pressure_from_altitude(float altitude_m) {
  return 101325.0f * std::pow(1.0f - (altitude_m / 44330.0f), 5.255f);
}

void update_visual_outputs() {
  const State current = g_fsm.state();
  digitalWrite(cansat::kPinLedLaunchPad,
               current == State::LaunchPad ? HIGH : LOW);
  digitalWrite(cansat::kPinLedAscent, current == State::Ascent ? HIGH : LOW);
  digitalWrite(cansat::kPinLedDescent, current == State::Descent ? HIGH : LOW);
  digitalWrite(cansat::kPinLedLanded, current == State::Landed ? HIGH : LOW);

  const bool para2_open =
      g_apogee.second_parachute_triggered() || g_mechanism_activated;
  digitalWrite(cansat::kPinLedSecondParachute, para2_open ? HIGH : LOW);
}

void pulse_servo(uint32_t now_ms) {
  if ((now_ms - g_last_servo_pulse_ms) < 20) {
    return;
  }
  g_last_servo_pulse_ms = now_ms;
  const bool para2_open =
      g_apogee.second_parachute_triggered() || g_mechanism_activated;
  const int angle_deg = para2_open ? 90 : 0;
  const int pulse_us = 544 + (angle_deg * (2400 - 544)) / 180;
  digitalWrite(cansat::kPinServoMechanism, HIGH);
  delayMicroseconds(pulse_us);
  digitalWrite(cansat::kPinServoMechanism, LOW);
}

void apply_sample(const SensorSample& sample) {
  g_frame.altitude_m = sample.altitude_m - g_persistent.altitude_offset_m;
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
  g_persistent.second_parachute_deployed =
      g_apogee.second_parachute_triggered() || g_mechanism_activated;
  g_storage.save(g_persistent);
}

void start_auto_flight(uint32_t now_ms) {
  g_fsm.on_event(Event::Reset);
  g_fsm.on_event(Event::Arm);
  g_apogee.reset();
  g_mechanism_activated = false;
  g_auto_flight_active = true;
  g_auto_flight_start_ms = now_ms;
  g_launch_start_ms = now_ms;
  std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "SIM");
  Serial.println("EVENT: AUTO_FLIGHT_START (0m -> 400m -> Apogeo -> 80% -> Landed)");
}

void update_flight_logic(uint32_t now_ms) {
  if (!g_fsm.armed()) {
    g_fsm.on_event(Event::Arm);
  }

  const float alt_m = g_frame.altitude_m;
  if (g_fsm.state() == State::LaunchPad && alt_m >= 15.0f) {
    if (g_fsm.on_event(Event::LaunchDetected)) {
      g_launch_start_ms = now_ms;
      Serial.println("EVENT: LAUNCH_DETECTED -> Ascent");
    }
  }

  if (g_fsm.state() == State::Ascent || g_fsm.state() == State::Descent) {
    const uint32_t flight_elapsed_ms =
        (now_ms >= g_launch_start_ms) ? (now_ms - g_launch_start_ms) : 0;
    const FlightEvent fev = g_apogee.update(alt_m, flight_elapsed_ms);

    if (fev == FlightEvent::ApogeeDetected) {
      g_fsm.on_event(Event::ApogeeDetected);
      Serial.print("EVENT: APOGEE_DETECTED -> Descent (Max Alt: ");
      Serial.print(g_apogee.max_altitude_m(), 1);
      Serial.println(" m)");
    } else if (fev == FlightEvent::SecondParachuteTriggered) {
      g_mechanism_activated = true;
      Serial.print("EVENT: SECOND_PARACHUTE_80_PERCENT -> Servo 90 deg (Alt: ");
      Serial.print(alt_m, 1);
      Serial.println(" m)");
    }
  }

  if (g_fsm.state() == State::Descent && alt_m <= 5.0f) {
    if (g_fsm.on_event(Event::LandedDetected)) {
      g_auto_flight_active = false;
      Serial.println("EVENT: LANDED_DETECTED -> Landed");
    }
  }
}

void handle_command(const char* line, uint32_t now_ms) {
  if ( std::strncmp(line, "SIM", 3) == 0) {
    start_auto_flight(now_ms);
    return;
  }
  if (std::strncmp(line, "RST", 3) == 0) {
    g_fsm.on_event(Event::Reset);
    g_fsm.on_event(Event::Arm);
    g_apogee.reset();
    g_mechanism_activated = false;
    g_auto_flight_active = false;
    std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "RST");
    Serial.println("RST OK");
    return;
  }

  const CommandType command = cansat::parse_command(line);
  switch (command) {
    case CommandType::Status:
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "CX");
      Serial.print("CX OK ");
      Serial.print(FlightStateMachine::state_name(g_fsm.state()));
      Serial.print(" PKT=");
      Serial.println(g_packet_count);
      break;
    case CommandType::SetTime:
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "ST");
      Serial.println("ST OK");
      break;
    case CommandType::CalibrateAltitude:
      g_persistent.altitude_offset_m += g_frame.altitude_m;
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "CAL");
      persist_state(g_frame.mission_time_ms);
      Serial.println("CAL OK");
      break;
    case CommandType::ActivateMechanism:
      g_mechanism_activated = true;
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "MEC");
      persist_state(g_frame.mission_time_ms);
      Serial.println("MEC OK (Servo liberado -> 90 deg)");
      break;
    case CommandType::None:
      break;
  }
}

}  // namespace

void setup() {
  Serial.begin(cansat::kSerialBaud);

  pinMode(cansat::kPinLaunchButton, INPUT_PULLUP);
  pinMode(cansat::kPinLedLaunchPad, OUTPUT);
  pinMode(cansat::kPinLedAscent, OUTPUT);
  pinMode(cansat::kPinLedDescent, OUTPUT);
  pinMode(cansat::kPinLedSecondParachute, OUTPUT);
  pinMode(cansat::kPinLedLanded, OUTPUT);
  pinMode(cansat::kPinServoMechanism, OUTPUT);

  Wire.begin(cansat::kI2cSdaPin, cansat::kI2cSclPin, cansat::kI2cClockHz);
  g_mpu_available = init_mpu6050();

  PersistentState restored{};
  if (g_storage.load(restored)) {
    g_packet_count = restored.packet_count;
    g_fsm.restore(restored.state, restored.armed);
    g_persistent = restored;
  } else {
    g_fsm.on_event(Event::Arm);
  }

  update_visual_outputs();
  g_last_telemetry_ms = millis();

  Serial.println("=========================================================");
  Serial.println("CanSat UTN FRRe - OBC ESP32-S3 (ISI 1: Florencia Gonzalez)");
  Serial.println("Controles en Wokwi:");
  Serial.println(" - Boton Verde o comando 'SIM': Vuelo simulado automatico");
  Serial.println(" - Slider Altitud: Control manual de altura (0 a 500 m)");
  Serial.println(" - Sensor MPU-6050: Clic para cambiar Accel/Gyro/Temp");
  Serial.println(" - Comandos Serie: CX | ST | CAL | MEC | SIM | RST");
  Serial.println("=========================================================");
}

void loop() {
  const uint32_t now = millis();

  // Boton verde: dispara o reinicia el vuelo simulado automatico
  const bool btn_pressed = (digitalRead(cansat::kPinLaunchButton) == LOW);
  if (btn_pressed && g_last_button_state) {
    start_auto_flight(now);
  }
  g_last_button_state = !btn_pressed;

  // Lectura de sensores base + IMU MPU-6050 + Slider de altitud
  SensorSample sample{};
  g_sensors.read(sample);
  sample.battery_v = 3.95f;
  sample.current_ma = 185.0f;
  sample.latitude_deg = -27.451200f;
  sample.longitude_deg = -58.986500f;
  sample.satellites = 8;

  read_mpu6050(sample);

  const int pot_raw = analogRead(cansat::kPinAltitudePot);
  if (g_last_pot_raw < 0) {
    g_last_pot_raw = pot_raw;
  } else if (std::abs(pot_raw - g_last_pot_raw) > 80) {
    // Si el usuario mueve el slider manualmente, toma control manual
    g_auto_flight_active = false;
    g_last_pot_raw = pot_raw;
  }

  if (g_auto_flight_active) {
    sample.altitude_m =
        compute_auto_flight_altitude(now - g_auto_flight_start_ms);
  } else {
    sample.altitude_m = (static_cast<float>(pot_raw) / 4095.0f) * 500.0f;
  }
  sample.pressure_pa = pressure_from_altitude(sample.altitude_m);
  sample.gps_altitude_m = sample.altitude_m;

  apply_sample(sample);
  update_flight_logic(now);
  update_visual_outputs();
  pulse_servo(now);

  if (Serial.available() > 0) {
    char command_line[64];
    const std::size_t length =
        Serial.readBytesUntil('\n', command_line, sizeof(command_line) - 1);
    command_line[length] = '\0';
    handle_command(command_line, now);
  }

  if ((now - g_last_telemetry_ms) >= cansat::kTelemetryPeriodMs) {
    g_last_telemetry_ms += cansat::kTelemetryPeriodMs;
    g_packet_count = cansat::next_packet_count(g_packet_count);

    std::snprintf(
        g_optional_buffer, sizeof(g_optional_buffer),
        "MAX:%.1f;P2:%u;CMD:%s", g_apogee.max_altitude_m(),
        (g_apogee.second_parachute_triggered() || g_mechanism_activated) ? 1u
                                                                         : 0u,
        g_last_cmd_echo);

    g_frame.team_id = cansat::kTeamId;
    g_frame.mission_time_ms = now;
    g_frame.packet_count = g_packet_count;
    g_frame.mode = g_auto_flight_active ? 1 : 0;
    g_frame.state = g_fsm.state();
    g_frame.optional_data = g_optional_buffer;

    char line[256];
    cansat::format_telemetry_csv(g_frame, line, sizeof(line));
    Serial.println(line);

    persist_state(now);
  }
}
