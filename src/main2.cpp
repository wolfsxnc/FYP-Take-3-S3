/*
#include <Arduino.h>
#include "config.h"
#include "modbus.h"
#include "lora.h"

void setup() {
  Serial.begin(115200);
  while (!Serial);
  

  initEthernet();
  initLoRa();
}

void loop() {
  unsigned long now = millis();

  if (!joined) {
    os_runloop_once();
    return;
  }

  checkAlarmUplink(); 

  if (now - lastModbusPoll >= MODBUS_SCAN_INTERVAL) {
    lastModbusPoll = now;
    pollModbus();
  }

  if (now - lastUplink >= LORA_UPLINK_INTERVAL) {
    lastUplink = now;
    sendLoRaUplink();
  }
}
  */