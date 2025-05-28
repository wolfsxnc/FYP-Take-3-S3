#pragma once
#include <Arduino.h>
#include "config.h"

enum InputType { DIGITAL, COUNTER };

struct InputConfig {
  uint8_t pin;
  InputType type;
  bool alarmActive;
  uint8_t alarmExpected;
  uint32_t counterValue = 0;
  bool lastState = false;
};

extern InputConfig inputConfigs[2];  // <-- This is the actual definition

void resetCounters();
void handleDigitalInputs();
void checkInputAlarms();