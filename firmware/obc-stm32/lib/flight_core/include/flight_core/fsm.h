#pragma once

#include <cstdint>

namespace cansat {

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
  FlightStateMachine();

  State state() const { return state_; }
  bool armed() const { return armed_; }

  bool on_event(Event event);
  void restore(State state, bool armed);

  static const char* state_name(State state);

 private:
  bool transition_to(State next);

  State state_;
  bool armed_;
};

}  // namespace cansat
