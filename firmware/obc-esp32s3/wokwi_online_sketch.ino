// ============================================================================
// CanSat Universitario 2026-2027 — UTN FRRe
// Autora: Florencia Gonzalez (ISI 1)
// Archivo unificado para Wokwi Online (https://wokwi.com/projects/new/esp32-s3)
//
// Instrucciones rapidas para Wokwi Online:
// 1. Pega todo este contenido en la pestaña "sketch.ino" de Wokwi Online.
// 2. Pega el contenido de "diagram.json" en la pestaña "diagram.json".
// 3. Presiona el boton verde "Start the simulation" (Play).
// ============================================================================

#include <Arduino.h>
#include <Wire.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace cansat {

// --- Configuracion de placa y pines (include/config.h) ---
constexpr const char* kBoardName = "ESP32-S3 DevKitC-1";
constexpr uint16_t kTeamId = 1000;
constexpr uint32_t kTelemetryPeriodMs = 1000;
constexpr uint32_t kSerialBaud = 115200;

constexpr uint8_t kI2cSdaPin = 8;
constexpr uint8_t kI2cSclPin = 9;
constexpr uint32_t kI2cClockHz = 400000;

constexpr float kMinApogeeAltitudeM = 100.0f;
constexpr float kApogeeDropM = 5.0f;
constexpr float kSecondParachuteRatio = 0.80f;
constexpr uint32_t kMinFlightTimeMs = 10000;

constexpr uint8_t kFramI2cAddress = 0x50;
constexpr uint8_t kRtcI2cAddress = 0x52;
constexpr uint8_t kMpu6050I2cAddress = 0x68;

constexpr uint8_t kPinAltitudePot = 4;          // Slider analogico de altitud (0 - 500 m)
constexpr uint8_t kPinLaunchButton = 5;         // Boton de armado / vuelo automatico
constexpr uint8_t kPinLedLaunchPad = 10;        // LED Azul: Estado LaunchPad (armado)
constexpr uint8_t kPinLedAscent = 11;           // LED Verde: Estado Ascent
constexpr uint8_t kPinLedDescent = 12;          // LED Amarillo: Estado Descent (Apogeo)
constexpr uint8_t kPinLedSecondParachute = 13;  // LED Rojo: 2do Paracaidas (80% alt max)
constexpr uint8_t kPinLedLanded = 14;           // LED Blanco: Estado Landed
constexpr uint8_t kPinServoMechanism = 15;      // Servo de liberacion del mecanismo

// --- Maquina de estados (flight_core/fsm) ---
enum class State : uint8_t {
  LaunchPad = 0,
  Ascent = 1,
  Descent = 2,
  Landed = 3,
};

enum class Event : uint8_t {
  Arm = 0,
  LaunchDetected = 1,
  ApogeeDetected = 2,
  LandedDetected = 3,
  Reset = 4,
};

class FlightStateMachine {
 public:
  FlightStateMachine() : state_(State::LaunchPad), armed_(false) {}

  State state() const { return state_; }
  bool armed() const { return armed_; }

  bool on_event(Event event) {
    switch (event) {
      case Event::Arm:
        armed_ = true;
        return true;
      case Event::LaunchDetected:
        if (armed_ && state_ == State::LaunchPad) {
          return transition_to(State::Ascent);
        }
        return false;
      case Event::ApogeeDetected:
        if (state_ == State::Ascent) {
          return transition_to(State::Descent);
        }
        return false;
      case Event::LandedDetected:
        if (state_ == State::Descent) {
          return transition_to(State::Landed);
        }
        return false;
      case Event::Reset:
        armed_ = false;
        return transition_to(State::LaunchPad);
    }
    return false;
  }

  void restore(State state, bool armed) {
    state_ = state;
    armed_ = armed;
  }

  static const char* state_name(State state) {
    switch (state) {
      case State::LaunchPad:
        return "LaunchPad";
      case State::Ascent:
        return "Ascent";
      case State::Descent:
        return "Descent";
      case State::Landed:
        return "Landed";
    }
    return "Unknown";
  }

 private:
  bool transition_to(State next) {
    if (state_ == next) {
      return false;
    }
    state_ = next;
    return true;
  }

  State state_;
  bool armed_;
};

// --- Detector de Apogeo y 80% (flight_core/flight) ---
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
  explicit ApogeeDetector(const ApogeeConfig& config = ApogeeConfig{})
      : config_(config) {}

  FlightEvent update(float altitude_m, uint32_t timestamp_ms) {
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

  void reset() {
    max_altitude_m_ = 0.0f;
    max_altitude_time_ms_ = 0;
    apogee_detected_ = false;
    second_parachute_triggered_ = false;
  }

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

// --- Sensores y Telemetria (flight_core/sensors & telemetry) ---
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

// --- Telecomandos (flight_core/commands) ---
enum class CommandType : uint8_t {
  None = 0,
  Status = 1,
  SetTime = 2,
  CalibrateAltitude = 3,
  ActivateMechanism = 4,
};

bool matches_cmd(const char* line, const char* token) {
  const std::size_t length = std::strlen(token);
  if (std::strncmp(line, token, length) != 0) {
    return false;
  }
  const char next = line[length];
  return next == '\0' || next == ' ' || next == '\r' || next == '\n';
}

CommandType parse_command(const char* line) {
  if (line == nullptr) return CommandType::None;
  if (matches_cmd(line, "CX")) return CommandType::Status;
  if (matches_cmd(line, "ST")) return CommandType::SetTime;
  if (matches_cmd(line, "CAL")) return CommandType::CalibrateAltitude;
  if (matches_cmd(line, "MEC")) return CommandType::ActivateMechanism;
  return CommandType::None;
}

}  // namespace cansat

// ============================================================================
// Logica principal del OBC en ESP32-S3
// ============================================================================
namespace {

using cansat::ApogeeDetector;
using cansat::CommandType;
using cansat::Event;
using cansat::FlightEvent;
using cansat::FlightStateMachine;
using cansat::SensorSample;
using cansat::State;
using cansat::TelemetryFrame;

FlightStateMachine g_fsm;
ApogeeDetector g_apogee({cansat::kMinApogeeAltitudeM, cansat::kApogeeDropM,
                         cansat::kSecondParachuteRatio,
                         cansat::kMinFlightTimeMs});
TelemetryFrame g_frame{};

uint32_t g_packet_count = 0;
uint32_t g_last_telemetry_ms = 0;
uint32_t g_last_servo_pulse_ms = 0;
uint32_t g_launch_start_ms = 0;
float g_altitude_offset_m = 0.0f;

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
  Wire.write(0x6B);
  Wire.write(0x00);
  return Wire.endTransmission() == 0;
}

bool read_mpu6050(SensorSample& sample) {
  if (!g_mpu_available) return false;
  Wire.beginTransmission(cansat::kMpu6050I2cAddress);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(cansat::kMpu6050I2cAddress, static_cast<uint8_t>(14)) < 14) {
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
  digitalWrite(cansat::kPinLedLaunchPad, current == State::LaunchPad ? HIGH : LOW);
  digitalWrite(cansat::kPinLedAscent, current == State::Ascent ? HIGH : LOW);
  digitalWrite(cansat::kPinLedDescent, current == State::Descent ? HIGH : LOW);
  digitalWrite(cansat::kPinLedLanded, current == State::Landed ? HIGH : LOW);

  const bool para2_open =
      g_apogee.second_parachute_triggered() || g_mechanism_activated;
  digitalWrite(cansat::kPinLedSecondParachute, para2_open ? HIGH : LOW);
}

void pulse_servo(uint32_t now_ms) {
  if ((now_ms - g_last_servo_pulse_ms) < 20) return;
  g_last_servo_pulse_ms = now_ms;
  const bool para2_open =
      g_apogee.second_parachute_triggered() || g_mechanism_activated;
  const int angle_deg = para2_open ? 90 : 0;
  const int pulse_us = 544 + (angle_deg * (2400 - 544)) / 180;
  digitalWrite(cansat::kPinServoMechanism, HIGH);
  delayMicroseconds(pulse_us);
  digitalWrite(cansat::kPinServoMechanism, LOW);
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
  if (std::strncmp(line, "SIM", 3) == 0) {
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
      g_altitude_offset_m += g_frame.altitude_m;
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "CAL");
      Serial.println("CAL OK");
      break;
    case CommandType::ActivateMechanism:
      g_mechanism_activated = true;
      std::snprintf(g_last_cmd_echo, sizeof(g_last_cmd_echo), "MEC");
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
  g_fsm.on_event(Event::Arm);
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

  const bool btn_pressed = (digitalRead(cansat::kPinLaunchButton) == LOW);
  if (btn_pressed && g_last_button_state) {
    start_auto_flight(now);
  }
  g_last_button_state = !btn_pressed;

  SensorSample sample{};
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

  g_frame.altitude_m = sample.altitude_m - g_altitude_offset_m;
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
  }
}
