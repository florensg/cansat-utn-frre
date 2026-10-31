#include "flight_core/fsm.h"

namespace cansat {

FlightStateMachine::FlightStateMachine() : state_(State::LaunchPad), armed_(false) {}

bool FlightStateMachine::on_event(Event event) {
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

void FlightStateMachine::restore(State state, bool armed) {
  state_ = state;
  armed_ = armed;
}

bool FlightStateMachine::transition_to(State next) {
  if (state_ == next) {
    return false;
  }
  state_ = next;
  return true;
}

const char* FlightStateMachine::state_name(State state) {
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

}  // namespace cansat
