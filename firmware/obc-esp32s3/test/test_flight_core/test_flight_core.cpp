#include <cstring>

#include <unity.h>

#include "flight_core/commands.h"
#include "flight_core/flight.h"
#include "flight_core/fsm.h"
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

void test_commands_parser(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::Status),
                        static_cast<int>(parse_command("CX")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::SetTime),
                        static_cast<int>(parse_command("ST 2026-10-31T12:00:00")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::CalibrateAltitude),
                        static_cast<int>(parse_command("CAL")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::ActivateMechanism),
                        static_cast<int>(parse_command("MEC")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::None),
                        static_cast<int>(parse_command("CX2")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::None),
                        static_cast<int>(parse_command(nullptr)));
  TEST_ASSERT_EQUAL_STRING("CAL",
                           command_name(CommandType::CalibrateAltitude));
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
  RUN_TEST(test_commands_parser);
  RUN_TEST(test_storage_roundtrip);
  RUN_TEST(test_storage_without_state);
  return UNITY_END();
}
