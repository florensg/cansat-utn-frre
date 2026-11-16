#include "status_output.h"

#include <Arduino.h>

#include "config.h"

namespace cansat {
namespace {

bool pin_assigned(uint8_t pin) { return pin != kPowerPinUnassigned; }

// Ultimo nivel escrito en cada LED (-1 = todavia no se escribio).
int g_last_ok = -1;
int g_last_fault = -1;

void drive(uint8_t pin, bool on, int& last) {
  if (!pin_assigned(pin)) {
    return;
  }
  const int level = on ? HIGH : LOW;
  if (last == level) {
    return;
  }
  digitalWrite(pin, level);
  last = level;
}

}  // namespace

void status_output_init() {
  if (pin_assigned(kPinStatusLedOk)) {
    pinMode(kPinStatusLedOk, OUTPUT);
    digitalWrite(kPinStatusLedOk, LOW);
  }
  if (pin_assigned(kPinStatusLedFault)) {
    pinMode(kPinStatusLedFault, OUTPUT);
    digitalWrite(kPinStatusLedFault, LOW);
  }
  g_last_ok = LOW;
  g_last_fault = LOW;
}

void status_output_apply(const StatusLeds& leds) {
  drive(kPinStatusLedOk, leds.ok, g_last_ok);
  drive(kPinStatusLedFault, leds.fault, g_last_fault);
}

}  // namespace cansat
