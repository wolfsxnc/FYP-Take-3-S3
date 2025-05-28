#include <LittleFS.h>
#include <flashfs.h>
#include <serial_editor.h>
#include <vector>

extern bool shellMode;


String commandHistory[HISTORY_SIZE];
int historyIndex = 0;
int currentHistoryPos = 0;

void executeCommand(const String& input) {
  String cmd = input;
  cmd.trim();

  if (cmd == "list") {
    Serial.println("📁 Files on LittleFS:");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
      Serial.printf(" - %s (%d bytes)\n", file.name(), file.size());
      file = root.openNextFile();
    }
    return;
  }

  if (cmd.startsWith("view ")) {
    String path = cmd.substring(5);
    File file = LittleFS.open(path.c_str(), "r");
    if (!file) {
      Serial.println("❌ File not found");
      return;
    }
    Serial.printf("📄 Contents of %s:\n", path.c_str());
    while (file.available()) Serial.write(file.read());
    Serial.println();
    file.close();
    return;
  }

  if (cmd.startsWith("edit ")) {
    String path = cmd.substring(5);
    editFile(path.c_str());
    return;
  }  
  

  if (cmd.startsWith("delete ")) {
    String path = cmd.substring(7);
    if (LittleFS.remove(path.c_str())) {
      Serial.println("🗑️ File deleted.");
    } else {
      Serial.println("❌ Delete failed.");
    }
    return;
  }

  if (cmd.startsWith("write ")) {
    String path = cmd.substring(6);
    Serial.println("✍️ Enter content (end with a single line 'EOF'):");
    File file = LittleFS.open(path.c_str(), "w");
    if (!file) {
      Serial.println("❌ Failed to open file");
      return;
    }

    while (true) {
      Serial.print("... ");
      while (!Serial.available()) delay(10);
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line == "EOF") break;
      file.println(line);
    }

    file.close();
    Serial.printf("✅ File saved to %s\n", path.c_str());
    return;
  }

  if (cmd == "shell") {
    shellMode = true;
    Serial.println("🖥️ Entering config shell. Type 'exit' to leave.");
    return;
  }
  
  if (cmd == "monitor") {
    shellMode = false;
    Serial.println("📈 Returning to monitoring mode.");
    return;
  }
  

  if (cmd == "help") {
    Serial.println("📖 Commands:");
    Serial.println("  list                - List all files");
    Serial.println("  view <file>         - View contents of a file");
    Serial.println("  write <file>        - Create/overwrite a file (end with EOF)");
    Serial.println("  edit <file>         - Edit file contents");
    Serial.println("  delete <file>       - Delete a file");
    Serial.println("  resetfs             - Format and regenerate config files");
    Serial.println("  reboot              - Reboot device");
    Serial.println("  reload              - Reload the filesystem");
    Serial.println("  shell               - Enable shell");
    Serial.println("  monitor             - Return to monitoring mode");
    Serial.println("  help                - Show this help");
    return;
  }

  if (cmd == "resetfs") {
    Serial.println("⚠️ Formatting filesystem...");
    if (LittleFS.format() && LittleFS.begin()) {
      Serial.println("✅ Reformatted. Creating default config...");
      writeDefaultConfigs();  // You should define this as in previous messages
    } else {
      Serial.println("❌ Failed to format LittleFS");
    }
    return;
  }

  if (cmd == "reboot") {
    Serial.println("🔄 Rebooting...");
    delay(1000);
    ESP.restart();
    return;
  }

  if (cmd == "reload") {
  Serial.println("🔁 Reloading configuration...");
  loadModbusConfigFromFlash();   // Reapplies Modbus, Ethernet, LoRa
  loadInputsConfig();   // Reapplies digital/counter input config
  Serial.println("✅ Configuration reloaded.");
  return;
}

  Serial.println("❓ Unknown command. Type 'help' for options.");
}


void editFile(const char* path) {
    File file = LittleFS.open(path, "r");
    if (!file) {
      Serial.println("❌ File not found.");
      return;
    }
  
    // Load file into a vector of lines
    std::vector<String> lines;
    while (file.available()) {
      lines.push_back(file.readStringUntil('\n'));
    }
    file.close();
  
    Serial.println("📝 Editing file:");
    for (size_t i = 0; i < lines.size(); i++) {
      Serial.printf(" %2d: %s\n", i, lines[i].c_str());
    }
  
    while (true) {
      String input = getUserLine("edit> ");
      input.trim();
      if (input.length() == 0) continue;
  
      // Fix formats like e4 into "e 4"
      if (input.startsWith("e") && input.length() > 1 && isdigit(input[1]) && input[2] != ' ') {
        input = "e " + input.substring(1);
      }
  
      if (input == "q") {
        Serial.println("👋 Exiting editor (no changes saved).");
        return;
      }
  
      if (input == "s") {
        // Attempt to save formatted JSON if valid
        String combined;
        for (auto& line : lines) {
          combined += line + "\n";
        }
  
        StaticJsonDocument<2048> doc;
        DeserializationError err = deserializeJson(doc, combined);
        File out = LittleFS.open(path, "w");
        if (!out) {
          Serial.println("❌ Failed to open file for writing");
          return;
        }
  
        if (!err) {
          serializeJsonPretty(doc, out);
          Serial.println("✅ File saved (formatted JSON).");
        } else {
          for (auto& line : lines) {
            out.println(line);
          }
          Serial.println("⚠️ File saved (raw lines, JSON invalid).");
        }
  
        out.close();
        return;
      }
  
      if (input == "a") {
        String newline = getUserLine("New line: ");
        lines.push_back(newline);
        Serial.println("➕ Line added.");
        continue;
      }
  
      if (input.startsWith("d ")) {
        int index = input.substring(2).toInt();
        if (index >= 0 && index < (int)lines.size()) {
          lines.erase(lines.begin() + index);
          Serial.println("🗑️ Line deleted.");
        } else {
          Serial.println("⚠️ Invalid line number.");
        }
        continue;
      }
  
      if (input.startsWith("e ")) {
        int index = input.substring(2).toInt();
        if (index >= 0 && index < (int)lines.size()) {
          Serial.printf("Current [%d]: %s\n", index, lines[index].c_str());
          String newLine = getUserLine("New: ");
          lines[index] = newLine;
          Serial.println("✅ Line updated");
        } else {
          Serial.println("⚠️ Invalid line number.");
        }
        continue;
      }

        else if (input.startsWith("i ")) {
    int index = input.substring(2).toInt();
    if (index >= 0 && index < lines.size()) {
      Serial.printf("Insert after [%d]:\n... ", index);
      String newLine = getUserLine();
      lines.insert(lines.begin() + index + 1, newLine);
      Serial.println("✅ Line inserted");
    }
  }

  
      Serial.println("❓ Commands: e <n>, d <n>, a, i <n>, s (save), q (quit)");
    }
  }
  
void handleSerialCommand() {
    static String inputBuffer = "";
    while (Serial.available()) {
      char c = Serial.read();
  
      if (c == '\n' || c == '\r') {
        if (inputBuffer.length() > 0) {
          Serial.println();  // Echo newline
          commandHistory[historyIndex] = inputBuffer;
          historyIndex = (historyIndex + 1) % HISTORY_SIZE;
          currentHistoryPos = historyIndex;  // Reset
          executeCommand(inputBuffer);  // your function
          inputBuffer = "";
          Serial.print(">>> ");
        }
      } else if (c == 127 || c == '\b') {  // Backspace
        if (inputBuffer.length() > 0) {
          inputBuffer.remove(inputBuffer.length() - 1);
          Serial.print("\b \b");
        }
      } else if (c == 27) {  // ESC
        if (Serial.peek() == '[') {
          Serial.read();  // skip '['
          char direction = Serial.read();
          if (direction == 'A') {  // Up
            currentHistoryPos = (currentHistoryPos - 1 + HISTORY_SIZE) % HISTORY_SIZE;
            inputBuffer = commandHistory[currentHistoryPos];
            Serial.print("\r>>> " + inputBuffer + "     \r>>> " + inputBuffer);
          } else if (direction == 'B') {  // Down
            currentHistoryPos = (currentHistoryPos + 1) % HISTORY_SIZE;
            inputBuffer = commandHistory[currentHistoryPos];
            Serial.print("\r>>> " + inputBuffer + "     \r>>> " + inputBuffer);
          }
        }
      } else {
        inputBuffer += c;
        Serial.print(c);
      }
    }
  }

  String getUserLine(const String& prompt) {
    String input;
    Serial.print(prompt);
    while (true) {
      if (Serial.available()) {
        char c = Serial.read();
  
        if (c == '\r') continue;  // Ignore carriage return
  
        if (c == '\n') {
          Serial.println();  // echo newline
          break;
        }
  
        if (c == 127 || c == '\b') {  // Handle backspace
          if (input.length() > 0) {
            input.remove(input.length() - 1);
            Serial.print("\b \b");
          }
        } else {
          input += c;
          Serial.print(c);  // echo typed char
        }
      }
    }
    return input;
  }
  
  

  void writeDefaultConfigs() {
    // Default Ethernet config
    if (!LittleFS.exists("/ethernet.json")) {
      String eth = R"json({
        "enableEthernet": true,
        "mac": [222, 173, 190, 239, 254, 238],
        "ethernet": {
          "dhcp": true,
          "ip": [192, 168, 0, 172],
          "gateway": [192, 168, 0, 1],
          "subnet": [255, 255, 255, 0],
          "dns": [8, 8, 8, 8]
        }
      })json";
      File file = LittleFS.open("/ethernet.json", "w");
      if (file) {
        file.print(eth);
        file.close();
        Serial.println("✅ Created /ethernet.json");
      }
    }
  
    // Default LoRa config
    if (!LittleFS.exists("/lora.json")) {
      String lora = R"json({
        "lora": {
          "interval": 10000,
          "subband": 4,
          "adr": true,
          "sf": 10,
          "deveui": [14, 122, 91, 17, 139, 63, 16, 43],
          "appeui": [143, 205, 206, 180, 6, 173, 188, 6],
          "appkey": [198, 101, 152, 196, 168, 144, 124, 56, 121, 10, 236, 43, 75, 91, 35, 65]
        }
      })json";
      File file = LittleFS.open("/lora.json", "w");
      if (file) {
        file.print(lora);
        file.close();
        Serial.println("✅ Created /lora.json");
      }
    }
  
    // Default Modbus config
    if (!LittleFS.exists("/modbus.json")) {
      String modbus = R"json({
        "interval": 5000,
        "requests": [
          {
            "ip": [192, 168, 0, 187],
            "unitID": 1,
            "start": 0,
            "count": 4,
            "function": 3,
            "alarms": [
              { "index": 0, "op": ">", "threshold": 1000 },
              { "index": 2, "op": "=", "threshold": 123 }
            ]
          }
        ]
      })json";
      File file = LittleFS.open("/modbus.json", "w");
      if (file) {
        file.print(modbus);
        file.close();
        Serial.println("✅ Created /modbus.json");
      }
    }
  }