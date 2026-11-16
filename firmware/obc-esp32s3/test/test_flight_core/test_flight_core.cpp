#include <cstddef>
#include <cstring>

#include <unity.h>

#include "config.h"
#include "flight_core/commands.h"
#include "flight_core/flight.h"
#include "flight_core/fsm.h"
#include "flight_core/mission_time.h"
#include "flight_core/power.h"
#include "flight_core/status.h"
#include "flight_core/storage.h"
#include "flight_core/telemetry.h"

using namespace cansat;

void setUp(void) {}

void tearDown(void) {}

void test_fsm_starts_on_launch_pad(void) {
  FlightStateMachine fsm;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(State::LaunchPad),
                        static_cast<int>(fsm.state()));
  TEST_ASSERT_FALSE(fsm.armed());
}

void test_fsm_requires_arm_before_launch(void) {
  FlightStateMachine fsm;
  TEST_ASSERT_FALSE(fsm.on_event(Event::LaunchDetected));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(State::LaunchPad),
                        static_cast<int>(fsm.state()));
}

void test_fsm_full_sequence(void) {
  FlightStateMachine fsm;
  TEST_ASSERT_TRUE(fsm.on_event(Event::Arm));
  TEST_ASSERT_TRUE(fsm.on_event(Event::LaunchDetected));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Ascent),
                        static_cast<int>(fsm.state()));
  TEST_ASSERT_TRUE(fsm.on_event(Event::ApogeeDetected));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Descent),
                        static_cast<int>(fsm.state()));
  TEST_ASSERT_TRUE(fsm.on_event(Event::LandedDetected));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Landed),
                        static_cast<int>(fsm.state()));
  TEST_ASSERT_FALSE(fsm.on_event(Event::LandedDetected));
}

void test_fsm_rejects_invalid_transition(void) {
  FlightStateMachine fsm;
  TEST_ASSERT_FALSE(fsm.on_event(Event::ApogeeDetected));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(State::LaunchPad),
                        static_cast<int>(fsm.state()));
}

void test_state_names(void) {
  TEST_ASSERT_EQUAL_STRING("LaunchPad",
                           FlightStateMachine::state_name(State::LaunchPad));
  TEST_ASSERT_EQUAL_STRING("Ascent",
                           FlightStateMachine::state_name(State::Ascent));
  TEST_ASSERT_EQUAL_STRING("Descent",
                           FlightStateMachine::state_name(State::Descent));
  TEST_ASSERT_EQUAL_STRING("Landed",
                           FlightStateMachine::state_name(State::Landed));
}

void test_telemetry_csv_format(void) {
  TelemetryFrame frame;
  frame.team_id = 1000;
  frame.mission_time_ms = 1234;
  frame.packet_count = 7;
  frame.mode = 1;
  frame.state = State::Ascent;
  frame.altitude_m = 120.5f;
  frame.temperature_c = 21.25f;
  frame.optional_data = "ATT=OK";

  char buffer[512];
  const std::size_t length = format_telemetry_csv(frame, buffer, sizeof(buffer));

  TEST_ASSERT_TRUE(length > 0);
  TEST_ASSERT_NOT_NULL(std::strstr(buffer, "1000,1234,7,1,Ascent,120.50"));
  TEST_ASSERT_NOT_NULL(std::strstr(buffer, "ATT=OK"));
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(std::strlen(buffer)),
                           static_cast<uint32_t>(length));
}

void test_telemetry_handles_small_buffer(void) {
  TelemetryFrame frame;
  char buffer[8];
  const std::size_t length = format_telemetry_csv(frame, buffer, sizeof(buffer));
  TEST_ASSERT_TRUE(length < sizeof(buffer));
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(std::strlen(buffer)),
                           static_cast<uint32_t>(length));
}

void test_packet_count_increments(void) {
  TEST_ASSERT_EQUAL_UINT32(1, next_packet_count(0));
  TEST_ASSERT_EQUAL_UINT32(43, next_packet_count(42));
}

void test_apogee_and_second_parachute(void) {
  ApogeeConfig config;
  config.min_flight_time_ms = 0;
  ApogeeDetector detector(config);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(FlightEvent::None),
                        static_cast<int>(detector.update(0.0f, 0)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FlightEvent::None),
                        static_cast<int>(detector.update(150.0f, 1000)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FlightEvent::None),
                        static_cast<int>(detector.update(149.0f, 2000)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FlightEvent::ApogeeDetected),
                        static_cast<int>(detector.update(144.0f, 3000)));
  TEST_ASSERT_TRUE(detector.apogee_detected());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FlightEvent::None),
                        static_cast<int>(detector.update(121.0f, 4000)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FlightEvent::SecondParachuteTriggered),
      static_cast<int>(detector.update(119.0f, 5000)));
  TEST_ASSERT_TRUE(detector.second_parachute_triggered());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FlightEvent::None),
                        static_cast<int>(detector.update(50.0f, 6000)));
}

void test_apogee_ignores_flight_before_min_time(void) {
  ApogeeConfig config;
  config.min_flight_time_ms = 10000;
  ApogeeDetector detector(config);

  detector.update(200.0f, 500);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FlightEvent::None),
                        static_cast<int>(detector.update(150.0f, 1000)));
  TEST_ASSERT_FALSE(detector.apogee_detected());
}

void test_power_policy_base_rails(void) {
  const PowerMask base = power_bit(PowerRail::Rtc) | power_bit(PowerRail::Fram) |
                         power_bit(PowerRail::Sensors) |
                         power_bit(PowerRail::Gps) | power_bit(PowerRail::Radio);
  const State states[] = {State::LaunchPad, State::Ascent, State::Descent,
                          State::Landed};
  for (State s : states) {
    const PowerMask mask = power_rails_for_state(s, false);
    TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(base),
                             static_cast<uint16_t>(mask & base));
  }
}

void test_power_payload_policy(void) {
  const PowerMask payload = power_bit(PowerRail::Rpi) |
                            power_bit(PowerRail::XiaoCenit) |
                            power_bit(PowerRail::XiaoNadir) |
                            power_bit(PowerRail::Servo) |
                            power_bit(PowerRail::Sec);
  // LaunchPad: carga util apagada hasta el comando RPI.
  TEST_ASSERT_EQUAL_UINT16(
      0, static_cast<uint16_t>(power_rails_for_state(State::LaunchPad, false) &
                               payload));
  // Con override de la GCS se enciende antes del lanzamiento.
  TEST_ASSERT_EQUAL_UINT16(
      static_cast<uint16_t>(payload),
      static_cast<uint16_t>(power_rails_for_state(State::LaunchPad, true) &
                            payload));
  // En vuelo siempre encendida, con o sin override.
  TEST_ASSERT_EQUAL_UINT16(
      static_cast<uint16_t>(payload),
      static_cast<uint16_t>(power_rails_for_state(State::Ascent, false) &
                            payload));
  TEST_ASSERT_EQUAL_UINT16(
      static_cast<uint16_t>(payload),
      static_cast<uint16_t>(power_rails_for_state(State::Descent, false) &
                            payload));
  // En Landed el override se ignora para ahorrar bateria.
  TEST_ASSERT_EQUAL_UINT16(
      0, static_cast<uint16_t>(power_rails_for_state(State::Landed, true) &
                               payload));
}

void test_power_mask_helpers(void) {
  TEST_ASSERT_EQUAL_UINT16(1u << 0, power_bit(PowerRail::Rtc));
  TEST_ASSERT_TRUE(power_mask_has(kPowerAll, PowerRail::Servo));
  TEST_ASSERT_FALSE(power_mask_has(0, PowerRail::Rtc));
  TEST_ASSERT_EQUAL_STRING("RADIO", power_rail_name(PowerRail::Radio));
  TEST_ASSERT_EQUAL_STRING("SERVO", power_rail_name(PowerRail::Servo));
  TEST_ASSERT_EQUAL_STRING("SEC", power_rail_name(PowerRail::Sec));
}

void test_commands_parser(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::Status),
                        static_cast<int>(parse_command("CX")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::SetTime),
                        static_cast<int>(parse_command("ST 2026-10-31T12:00:00")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::CalibrateAltitude),
                        static_cast<int>(parse_command("CAL")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::ActivateMechanism),
                        static_cast<int>(parse_command("MEC")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::RpiOn),
                        static_cast<int>(parse_command("RPI")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::RpiOff),
                        static_cast<int>(parse_command("RPIOFF")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::RpiOn),
                        static_cast<int>(parse_command("RPI\r\n")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::None),
                        static_cast<int>(parse_command("RPION")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::None),
                        static_cast<int>(parse_command("$RPI,1,2")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::None),
                        static_cast<int>(parse_command("CX2")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::None),
                        static_cast<int>(parse_command(nullptr)));
  TEST_ASSERT_EQUAL_STRING("CAL",
                           command_name(CommandType::CalibrateAltitude));
  TEST_ASSERT_EQUAL_STRING("RPI", command_name(CommandType::RpiOn));
  TEST_ASSERT_EQUAL_STRING("RPIOFF", command_name(CommandType::RpiOff));
}

void test_storage_roundtrip(void) {
  MemoryStorage storage;
  PersistentState state;
  state.packet_count = 123;
  state.mission_time_ms = 456789;
  state.altitude_offset_m = 12.5f;
  state.state = State::Descent;
  state.armed = true;
  state.second_parachute_deployed = true;

  TEST_ASSERT_TRUE(storage.save(state));

  PersistentState restored;
  TEST_ASSERT_TRUE(storage.load(restored));
  TEST_ASSERT_EQUAL_UINT32(123, restored.packet_count);
  TEST_ASSERT_EQUAL_UINT32(456789, restored.mission_time_ms);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Descent),
                        static_cast<int>(restored.state));
  TEST_ASSERT_TRUE(restored.armed);
  TEST_ASSERT_TRUE(restored.second_parachute_deployed);
}

void test_storage_without_state(void) {
  MemoryStorage storage;
  PersistentState state;
  TEST_ASSERT_FALSE(storage.load(state));
}

// Guarda de regresion del pinmap v.2: las 5 llaves de alimentacion no pueden
// compartir GPIO entre si ni con las UART de GPS, XBee o Raspberry Pi.
void test_pinmap_has_no_uart_conflict(void) {
  const uint8_t pins[] = {kPowerPinRpi,     kPowerPinXiaoCenit,
                          kPowerPinXiaoNadir, kPowerPinServo,
                          kPowerPinSec};
  const uint8_t uart_pins[] = {kGpsRxPin, kGpsTxPin, kXbeeRxPin,
                               kXbeeTxPin, kRpiRxPin, kRpiTxPin};
  const std::size_t en_count = sizeof(pins) / sizeof(pins[0]);
  const std::size_t uart_count = sizeof(uart_pins) / sizeof(uart_pins[0]);

  for (std::size_t i = 0; i < en_count; ++i) {
    TEST_ASSERT_NOT_EQUAL(kPowerPinUnassigned, pins[i]);
    for (std::size_t j = i + 1; j < en_count; ++j) {
      TEST_ASSERT_NOT_EQUAL(pins[i], pins[j]);
    }
    for (std::size_t k = 0; k < uart_count; ++k) {
      TEST_ASSERT_NOT_EQUAL(uart_pins[k], pins[i]);
    }
  }

  TEST_ASSERT_TRUE(kPowerGatingEnabled);
}

// ---------------------------------------------------------------------------
// Fase 2: reloj de mision y RTC (logica pura, sin hardware).
// ---------------------------------------------------------------------------
void test_time_bcd_helpers(void) {
  TEST_ASSERT_EQUAL_UINT8(59, bcd_to_bin(0x59));
  TEST_ASSERT_EQUAL_UINT8(0x59, bin_to_bcd(59));
  TEST_ASSERT_EQUAL_UINT8(0x00, bin_to_bcd(0));
  TEST_ASSERT_EQUAL_UINT8(0x23, bin_to_bcd(23));
  TEST_ASSERT_TRUE(bcd_range_ok(0x59, 0, 59));
  TEST_ASSERT_TRUE(bcd_range_ok(0x23, 0, 23));
  // Nibble bajo invalido (A), valor fuera de rango y cero cuando el minimo es 1.
  TEST_ASSERT_FALSE(bcd_range_ok(0x6A, 0, 59));
  TEST_ASSERT_FALSE(bcd_range_ok(0x60, 0, 59));
  TEST_ASSERT_FALSE(bcd_range_ok(0x00, 1, 7));
}

void test_time_calendar_rules(void) {
  TEST_ASSERT_TRUE(is_leap_year(2000));
  TEST_ASSERT_FALSE(is_leap_year(2100));
  TEST_ASSERT_TRUE(is_leap_year(2024));
  TEST_ASSERT_FALSE(is_leap_year(2026));
  TEST_ASSERT_EQUAL_UINT8(29, days_in_month(2024, 2));
  TEST_ASSERT_EQUAL_UINT8(28, days_in_month(2026, 2));
  TEST_ASSERT_EQUAL_UINT8(30, days_in_month(2026, 4));
  TEST_ASSERT_EQUAL_UINT8(31, days_in_month(2026, 12));
  TEST_ASSERT_EQUAL_UINT8(0, days_in_month(2026, 13));
}

void test_time_validation(void) {
  DateTime dt;
  dt.year = 2026;
  dt.month = 11;
  dt.day = 16;
  dt.hour = 12;
  dt.minute = 34;
  dt.second = 56;
  TEST_ASSERT_TRUE(datetime_is_valid(dt));

  dt.month = 13;
  TEST_ASSERT_FALSE(datetime_is_valid(dt));
  dt.month = 11;
  dt.day = 32;
  TEST_ASSERT_FALSE(datetime_is_valid(dt));
  dt.day = 16;
  dt.hour = 24;
  TEST_ASSERT_FALSE(datetime_is_valid(dt));
  dt.hour = 12;

  // 29 de febrero: valido solo en bisiesto.
  dt.month = 2;
  dt.day = 29;
  TEST_ASSERT_TRUE(datetime_is_valid(dt));
  dt.year = 2026;
  TEST_ASSERT_FALSE(datetime_is_valid(dt));
}

void test_time_epoch_roundtrip(void) {
  DateTime dt;
  dt.year = 2026;
  dt.month = 11;
  dt.day = 16;
  dt.hour = 12;
  dt.minute = 34;
  dt.second = 56;

  const DateTime back = seconds_to_datetime(datetime_to_seconds(dt));
  TEST_ASSERT_EQUAL_UINT16(2026, back.year);
  TEST_ASSERT_EQUAL_UINT8(11, back.month);
  TEST_ASSERT_EQUAL_UINT8(16, back.day);
  TEST_ASSERT_EQUAL_UINT8(12, back.hour);
  TEST_ASSERT_EQUAL_UINT8(34, back.minute);
  TEST_ASSERT_EQUAL_UINT8(56, back.second);

  // El origen de la escala es 2000-01-01T00:00:00Z.
  TEST_ASSERT_EQUAL_UINT32(0, datetime_to_seconds(DateTime{}));
  DateTime jan2;
  jan2.year = 2000;
  jan2.month = 1;
  jan2.day = 2;
  TEST_ASSERT_EQUAL_UINT32(86400u, datetime_to_seconds(jan2));
}

void test_time_weekday_iso(void) {
  DateTime dt;
  dt.year = 2000;
  dt.month = 1;
  dt.day = 1;
  TEST_ASSERT_EQUAL_UINT8(6, weekday_iso(dt));  // sabado

  dt.year = 2026;
  dt.month = 11;
  dt.day = 16;
  TEST_ASSERT_EQUAL_UINT8(1, weekday_iso(dt));  // lunes

  dt.month = 11;
  dt.day = 15;
  TEST_ASSERT_EQUAL_UINT8(7, weekday_iso(dt));  // domingo
}

void test_time_source_precedence(void) {
  // Sin fuente previa, cualquier fuente real sirve.
  TEST_ASSERT_TRUE(time_source_accepts(TimeSource::None, TimeSource::Rtc, false));
  TEST_ASSERT_TRUE(time_source_accepts(TimeSource::None, TimeSource::Gps, false));
  // GPS > RTC.
  TEST_ASSERT_TRUE(time_source_accepts(TimeSource::Rtc, TimeSource::Gps, false));
  TEST_ASSERT_FALSE(time_source_accepts(TimeSource::Gps, TimeSource::Rtc, false));
  // millis() no puede pisar una fuente real.
  TEST_ASSERT_FALSE(time_source_accepts(TimeSource::Gps, TimeSource::Millis, false));
  TEST_ASSERT_FALSE(time_source_accepts(TimeSource::Rtc, TimeSource::Millis, false));
  TEST_ASSERT_FALSE(time_source_accepts(TimeSource::Gps, TimeSource::None, false));
  // El comando manual siempre entra, incluso sobre GPS.
  TEST_ASSERT_TRUE(time_source_accepts(TimeSource::Gps, TimeSource::Command, false));
  // Dentro de la ventana de gracia el GPS no revierte la correccion manual.
  TEST_ASSERT_FALSE(time_source_accepts(TimeSource::Command, TimeSource::Gps, true));
  TEST_ASSERT_TRUE(time_source_accepts(TimeSource::Command, TimeSource::Gps, false));
  // El RTC nunca reemplaza una correccion manual.
  TEST_ASSERT_FALSE(time_source_accepts(TimeSource::Command, TimeSource::Rtc, false));
}

void test_mission_clock_ticks(void) {
  MissionClock clock;
  TEST_ASSERT_FALSE(clock.synced());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TimeSource::None),
                        static_cast<int>(clock.source()));

  DateTime dt;
  dt.year = 2026;
  dt.month = 11;
  dt.day = 16;
  dt.hour = 12;
  dt.minute = 0;
  dt.second = 0;
  clock.sync(TimeSource::Command, dt, 1000);

  TEST_ASSERT_TRUE(clock.synced());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TimeSource::Command),
                        static_cast<int>(clock.source()));

  uint8_t h = 0;
  uint8_t m = 0;
  uint8_t s = 0;
  uint8_t valid = 0;
  clock.get_hms(1000, h, m, s, valid);
  TEST_ASSERT_EQUAL_UINT8(12, h);
  TEST_ASSERT_EQUAL_UINT8(0, m);
  TEST_ASSERT_EQUAL_UINT8(0, s);
  TEST_ASSERT_EQUAL_UINT8(1, valid);

  // 61 s despues el reloj sigue contando con millis().
  clock.get_hms(62000, h, m, s, valid);
  TEST_ASSERT_EQUAL_UINT8(12, h);
  TEST_ASSERT_EQUAL_UINT8(1, m);
  TEST_ASSERT_EQUAL_UINT8(1, s);
}

void test_mission_clock_keeps_gps_over_rtc(void) {
  MissionClock clock;
  DateTime gps_time;
  gps_time.year = 2026;
  gps_time.month = 11;
  gps_time.day = 16;
  gps_time.hour = 10;
  clock.sync(TimeSource::Gps, gps_time, 0);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TimeSource::Gps),
                        static_cast<int>(clock.source()));

  // El RTC llega con una hora vieja: se descarta sin tocar el ancla.
  DateTime rtc_time = gps_time;
  rtc_time.hour = 9;
  clock.sync(TimeSource::Rtc, rtc_time, 1000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TimeSource::Gps),
                        static_cast<int>(clock.source()));

  uint8_t h = 0;
  uint8_t m = 0;
  uint8_t s = 0;
  uint8_t valid = 0;
  clock.get_hms(1000, h, m, s, valid);
  TEST_ASSERT_EQUAL_UINT8(10, h);
  TEST_ASSERT_EQUAL_UINT8(0, m);
  TEST_ASSERT_EQUAL_UINT8(1, s);
}

void test_mission_clock_command_hold(void) {
  MissionClock clock;
  DateTime gps_time;
  gps_time.year = 2026;
  gps_time.month = 11;
  gps_time.day = 16;
  gps_time.hour = 10;
  gps_time.minute = 0;
  gps_time.second = 0;
  clock.sync(TimeSource::Gps, gps_time, 0);

  // Correccion manual de +5 min desde tierra.
  DateTime cmd_time = gps_time;
  cmd_time.minute = 5;
  clock.sync(TimeSource::Command, cmd_time, 1000);
  TEST_ASSERT_TRUE(clock.command_hold_active(2000));

  // Dentro de la ventana (5 s) el GPS no la revierte.
  DateTime gps_again = gps_time;
  gps_again.second = 1;
  clock.sync(TimeSource::Gps, gps_again, 2000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TimeSource::Command),
                        static_cast<int>(clock.source()));

  uint8_t h = 0;
  uint8_t m = 0;
  uint8_t s = 0;
  uint8_t valid = 0;
  clock.get_hms(2000, h, m, s, valid);
  TEST_ASSERT_EQUAL_UINT8(10, h);
  TEST_ASSERT_EQUAL_UINT8(5, m);

  // Vencida la ventana, el GPS retoma el control.
  clock.sync(TimeSource::Gps, gps_again, 7000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(TimeSource::Gps),
                        static_cast<int>(clock.source()));
  TEST_ASSERT_FALSE(clock.command_hold_active(7000));
}

// ---------------------------------------------------------------------------
// Fase 3: salud del OBC y LED de estado (logica pura, sin hardware).
// ---------------------------------------------------------------------------
void test_status_severity_ok(void) {
  SystemHealth health;
  health.imu_ok = true;
  health.baro_ok = true;
  health.ina_ok = true;
  health.fram_ok = true;
  health.rtc_ok = true;
  health.rtc_power_loss = false;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemSeverity::Ok),
                        static_cast<int>(status_severity(health)));
}

void test_status_severity_degraded(void) {
  SystemHealth health;
  health.imu_ok = true;
  health.baro_ok = true;
  health.ina_ok = true;
  health.fram_ok = true;
  health.rtc_ok = true;

  // Falta la IMU: el vuelo sigue, pero se pierde actitud.
  health.imu_ok = false;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemSeverity::Degraded),
                        static_cast<int>(status_severity(health)));
  health.imu_ok = true;

  // RTC presente pero con PORF: la hora se perdio hasta que llegue GPS/ST.
  health.rtc_power_loss = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemSeverity::Degraded),
                        static_cast<int>(status_severity(health)));
  health.rtc_power_loss = false;

  // Sin RTC en el bus tambien es degradado (F2 queda en manos de GPS/ST).
  health.rtc_ok = false;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemSeverity::Degraded),
                        static_cast<int>(status_severity(health)));
}

void test_status_severity_fault(void) {
  // Sin barometro no hay altitud: falla critica aunque el resto este bien.
  SystemHealth health;
  health.imu_ok = true;
  health.ina_ok = true;
  health.fram_ok = true;
  health.rtc_ok = true;
  health.baro_ok = false;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemSeverity::Fault),
                        static_cast<int>(status_severity(health)));

  // Con barometro, lo que falta es degradacion, no falla.
  health.baro_ok = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemSeverity::Degraded),
                        static_cast<int>(status_severity(health)));
}

void test_status_leds_patterns(void) {
  // Ok: LED OK fijo en cualquier instante.
  const StatusLeds ok_0 = status_leds(SystemSeverity::Ok, 0);
  TEST_ASSERT_TRUE(ok_0.ok);
  TEST_ASSERT_FALSE(ok_0.fault);
  const StatusLeds ok_late = status_leds(SystemSeverity::Ok, 777);
  TEST_ASSERT_TRUE(ok_late.ok);

  // Degradado: LED OK parpadea a 1 Hz (500 ms encendido / 500 ms apagado).
  TEST_ASSERT_TRUE(status_leds(SystemSeverity::Degraded, 0).ok);
  TEST_ASSERT_FALSE(status_leds(SystemSeverity::Degraded, 500).ok);
  TEST_ASSERT_TRUE(status_leds(SystemSeverity::Degraded, 1000).ok);
  TEST_ASSERT_FALSE(status_leds(SystemSeverity::Degraded, 1200).fault);

  // Falla: OK apagado y LED de falla parpadeando.
  const StatusLeds fault_on = status_leds(SystemSeverity::Fault, 0);
  TEST_ASSERT_FALSE(fault_on.ok);
  TEST_ASSERT_TRUE(fault_on.fault);
  TEST_ASSERT_FALSE(status_leds(SystemSeverity::Fault, 500).fault);
  TEST_ASSERT_TRUE(status_leds(SystemSeverity::Fault, 1000).fault);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_fsm_starts_on_launch_pad);
  RUN_TEST(test_fsm_requires_arm_before_launch);
  RUN_TEST(test_fsm_full_sequence);
  RUN_TEST(test_fsm_rejects_invalid_transition);
  RUN_TEST(test_state_names);
  RUN_TEST(test_telemetry_csv_format);
  RUN_TEST(test_telemetry_handles_small_buffer);
  RUN_TEST(test_packet_count_increments);
  RUN_TEST(test_apogee_and_second_parachute);
  RUN_TEST(test_apogee_ignores_flight_before_min_time);
  RUN_TEST(test_power_policy_base_rails);
  RUN_TEST(test_power_payload_policy);
  RUN_TEST(test_power_mask_helpers);
  RUN_TEST(test_commands_parser);
  RUN_TEST(test_storage_roundtrip);
  RUN_TEST(test_storage_without_state);
  RUN_TEST(test_pinmap_has_no_uart_conflict);
  RUN_TEST(test_time_bcd_helpers);
  RUN_TEST(test_time_calendar_rules);
  RUN_TEST(test_time_validation);
  RUN_TEST(test_time_epoch_roundtrip);
  RUN_TEST(test_time_weekday_iso);
  RUN_TEST(test_time_source_precedence);
  RUN_TEST(test_mission_clock_ticks);
  RUN_TEST(test_mission_clock_keeps_gps_over_rtc);
  RUN_TEST(test_mission_clock_command_hold);
  RUN_TEST(test_status_severity_ok);
  RUN_TEST(test_status_severity_degraded);
  RUN_TEST(test_status_severity_fault);
  RUN_TEST(test_status_leds_patterns);
  return UNITY_END();
}
