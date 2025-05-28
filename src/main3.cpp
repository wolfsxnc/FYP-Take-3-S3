/*
#include <Arduino.h>
#include <LittleFS.h>
#include <serial_editor.h>

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!LittleFS.begin()) {
    Serial.println("❌ LittleFS init failed, formatting...");
    LittleFS.format();
    LittleFS.begin();
  }

  Serial.println("✅ LittleFS mounted");
  Serial.println("🖥️ Config shell ready. Type 'help'");
  Serial.print(">>> ");
}

void loop() {
  handleSerialCommand();  // <-- process input without blocking
}
  */
