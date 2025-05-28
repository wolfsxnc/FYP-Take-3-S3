#include <Arduino.h>
#include "config.h"
#include "flashfs.h"
#include "modbus.h"
#include "lora.h"
#include "serial_editor.h"
#include "inputs.h"

bool shellMode = false;
unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Mount FS and load config
  if (!initFlashFS()) {
    Serial.println("⚠️ Filesystem init failed. Running with default settings.");
  }

  loadModbusConfigFromFlash("/modbus.json");
  loadInputsConfig();
  initEthernet();
  Serial.printf("JOIN_MODE_ABP: %s\n", JOIN_MODE_ABP ? "true" : "false");
  initLoRa();

  Serial.println("🖥️ Type 'shell' to enter config editor.");
  Serial.println("ℹ️  Type 'monitor' to return to normal output.");
  Serial.print(">>> ");
}

void loop() {
  handleSerialCommand();  // Always parse input (only acts when shellMode=true)

  if (shellMode) {
    // Skip all other operations when in shell
    return;
  }

  unsigned long now = millis();

  if (!joined) {
    os_runloop_once();
    return;
  }

  checkAlarmUplink(); 
  
  handleDigitalInputs();

  if (now - lastModbusPoll >= MODBUS_SCAN_INTERVAL) {
    Serial.printf("Modbus Poll interval %lu ms\n", now - lastModbusPoll);
    lastModbusPoll = now;
    Serial.printf("🛰️ Modbus poll triggered at: %lu ms\n", millis());
    pollModbus();
    Serial.printf("✅ Modbus poll complete at: %lu ms\n", millis());
  }

  if (now - lastUplink >= LORA_UPLINK_INTERVAL) {
  Serial.printf("Lora interval %lu ms\n", now -lastUplink);
  lastUplink = now;
  unsigned long txStart = millis();
  Serial.printf("🛰️ Uplink triggered at: %lu ms\n", txStart);
  sendLoRaUplink();
  Serial.printf("✅ Uplink complete at: %lu ms\n", millis());
  }
}
