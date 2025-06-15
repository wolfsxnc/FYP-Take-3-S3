#include "inputs.h"
#include "lora.h"  // For alarmUplink() or sendAlarmUplink()

InputConfig inputConfigs[2]; 

void handleDigitalInputs() {
  for (int i = 0; i < 2; i++) {
    bool state = digitalRead(inputConfigs[i].pin);

    if (inputConfigs[i].type == COUNTER) {
      if (state && !inputConfigs[i].lastState) {
        inputConfigs[i].counterValue++;
      }
    }

    inputConfigs[i].lastState = state;

    if (inputConfigs[i].alarmActive && inputConfigs[i].type == DIGITAL) {
      if (state == inputConfigs[i].alarmExpected) {
        Serial.printf("Alarm: input %d = %d\n", inputConfigs[i].pin, state);
        // mark for alarm uplink
      }
    }
  }
  checkInputAlarms();
}

void resetCounters() {
  for (int i = 0; i < 2; i++) {
    if (inputConfigs[i].type == COUNTER) {
      inputConfigs[i].counterValue = 0;
    }
  }
}


void checkInputAlarms() {
  for (int i = 0; i < 2; i++) {
    InputConfig& cfg = inputConfigs[i];
    uint8_t actual = (cfg.type == COUNTER) ? (cfg.counterValue > 0 ? 1 : 0) : (cfg.lastState ? 1 : 0);

    if (actual != cfg.alarmExpected && !cfg.alarmActive) {
      cfg.alarmActive = true;
      sendAlarmUplink(i, cfg.alarmExpected, actual);
    } else if (actual == cfg.alarmExpected && cfg.alarmActive) {
      cfg.alarmActive = false;
    }
  }
}


