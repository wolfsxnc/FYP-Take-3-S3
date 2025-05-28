#include "flashfs.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "inputs.h"
#include <lmic.h>

bool initFlashFS() {
  if (!LittleFS.begin()) {
    Serial.println("❌ LittleFS mount failed, formatting...");
    if (LittleFS.format()) {
      Serial.println("✅ Format OK — mounting...");
      if (LittleFS.begin()) {
        Serial.println("✅ Mounted after format");
        return true;
      }
    }
    Serial.println("❌ Format failed");
    return false;
  }

  Serial.println("✅ LittleFS mounted");
  return true;
}


bool parseHexString(const String& hex, uint8_t* output, size_t len) {
  if (hex.length() != len * 2) return false;
  for (size_t i = 0; i < len; i++) {
    char high = hex.charAt(i * 2);
    char low  = hex.charAt(i * 2 + 1);
    output[i] = strtoul((String() + high + low).c_str(), nullptr, 16);
  }
  return true;
}



bool fileExistsFS(const char* path) {
  return LittleFS.exists(path);
}

String readFileFS(const char* path) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("❌ Failed to open %s\n", path);
    return "";
  }

  String content;
  while (file.available()) {
    content += (char)file.read();
  }
  file.close();
  return content;
}

bool writeFileFS(const char* path, const String& content) {
  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.printf("❌ Failed to open %s for write\n", path);
    return false;
  }
  file.print(content);
  file.close();
  Serial.printf("✅ File written: %s\n", path);
  return true;
}

bool appendToFileFS(const char* path, const String& content) {
  File file = LittleFS.open(path, "a");
  if (!file) {
    Serial.printf("❌ Failed to open %s for append\n", path);
    return false;
  }
  file.print(content);
  file.close();
  Serial.printf("✅ Content appended to: %s\n", path);
  return true;
}

void configureMACAddressFromJson(JsonObject doc) {
  if (!doc.containsKey("mac")) return;
  JsonArray mac = doc["mac"].as<JsonArray>();
  if (mac.size() != 6) {
    Serial.println("⚠️ MAC array must have 6 bytes");
    return;
  }
  for (int i = 0; i < 6; i++) MAC_ADDR[i] = mac[i];
  Serial.print("🧭 MAC address set to: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", MAC_ADDR[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}


void configureLoRaFromJson(JsonObject doc) {
  if (!doc.containsKey("lora")) return;
  JsonObject lora = doc["lora"];

  // Join mode: "otaa" or "abp"
  if (lora.containsKey("join")) {
    String mode = lora["join"].as<String>();
    JOIN_MODE_ABP = (mode == "abp");
    Serial.printf("🔄 Join mode: %s\n", JOIN_MODE_ABP ? "ABP" : "OTAA");
  }

  // OTAA credentials
  if (lora.containsKey("deveui")) {
    String s = lora["deveui"].as<String>();
    if (!parseHexString(s, DEVEUI, 8)) Serial.println("⚠️ Invalid DEVEUI");
  }
  if (lora.containsKey("appeui")) {
    String s = lora["appeui"].as<String>();
    if (!parseHexString(s, APPEUI, 8)) Serial.println("⚠️ Invalid APPEUI");
  }
  if (lora.containsKey("appkey")) {
    String s = lora["appkey"].as<String>();
    if (!parseHexString(s, APPKEY, 16)) Serial.println("⚠️ Invalid APPKEY");
  }

  // ABP credentials
  if (lora.containsKey("nwkskey")) {
    String s = lora["nwkskey"].as<String>();
    if (!parseHexString(s, NWKSKEY, 16)) Serial.println("⚠️ Invalid NWKSKEY");
  }
  if (lora.containsKey("appskey")) {
    String s = lora["appskey"].as<String>();
    if (!parseHexString(s, APPSKEY, 16)) Serial.println("⚠️ Invalid APPSKEY");
  }
  if (lora.containsKey("devaddr")) {
    String s = lora["devaddr"].as<String>();
    if (s.length() == 8) {
      DEVADDR = strtoul(s.c_str(), nullptr, 16);
    } else {
      Serial.println("⚠️ Invalid DEVADDR length");
    }
  }

  if (lora.containsKey("interval")) {
    LORA_UPLINK_INTERVAL = lora["interval"].as<unsigned long>();
    Serial.printf("⏱️ LoRa uplink interval set to %lu ms\n", LORA_UPLINK_INTERVAL);
  }
  if (lora.containsKey("subband")) {
    LORA_SUBBAND = lora["subband"].as<uint8_t>();
    Serial.printf("📶 LoRa subband set to %u\n", LORA_SUBBAND);
  }
  if (lora.containsKey("adr")) {
    LORA_ADR = lora["adr"].as<bool>();
    Serial.printf("⚙️ LoRa ADR %s\n", LORA_ADR ? "enabled" : "disabled");
  }  
  if (lora.containsKey("sf")) {
    LORA_SF = lora["sf"].as<uint8_t>();
    Serial.printf("📡 LoRa spreading factor set to SF%d\n", LORA_SF);
  }

  Serial.println("📡 LoRa config loaded from lora.json");
}

void configureEthernetFromJson(JsonObject doc) {
    // Optional: allow top-level "enableEthernet" flag
    if (doc.containsKey("enableEthernet")) {
      enableEthernet = doc["enableEthernet"].as<bool>();
      Serial.printf("🔧 Ethernet %s via config\n", enableEthernet ? "enabled" : "disabled");
    }
  
    if (!doc.containsKey("ethernet")) return;
  
    JsonObject eth = doc["ethernet"];
  
    if (!(eth["ip"].is<JsonArray>() && eth["gateway"].is<JsonArray>() && 
          eth["subnet"].is<JsonArray>() && eth["dns"].is<JsonArray>())) {
      Serial.println("⚠️ Ethernet IP configuration arrays are malformed");
      return;
    }
  
    if (!(eth["ip"].size() == 4 && eth["gateway"].size() == 4 && 
          eth["subnet"].size() == 4 && eth["dns"].size() == 4)) {
      Serial.println("⚠️ IP/gateway/subnet/dns must each be 4 bytes");
      return;
    }
  
    ETH_IP = IPAddress(eth["ip"][0], eth["ip"][1], eth["ip"][2], eth["ip"][3]);
    ETH_GATEWAY = IPAddress(eth["gateway"][0], eth["gateway"][1], eth["gateway"][2], eth["gateway"][3]);
    ETH_SUBNET = IPAddress(eth["subnet"][0], eth["subnet"][1], eth["subnet"][2], eth["subnet"][3]);
    ETH_DNS = IPAddress(eth["dns"][0], eth["dns"][1], eth["dns"][2], eth["dns"][3]);
  
    useDHCP = eth["dhcp"] | false;
  }
  

void loadModbusConfigFromFlash(const char* configPath) {
  const char* netConfigPath = "/ethernet.json";
  const char* loraConfigPath = "/lora.json";

  // --- Write default ethernet.json if missing ---
  if (!fileExistsFS(netConfigPath)) {
    String defaultEth = R"json({
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
    writeFileFS(netConfigPath, defaultEth);
    Serial.println("✍️ Default ethernet.json created (Flash)");
  }

  // --- Write default lora.json if missing ---
  if (!fileExistsFS(loraConfigPath)) {
      String defaultLora = R"json({
        "lora": {
          "join": "abp",
          "interval": 10000,
          "subband": 4,
          "adr": true,
          "sf": 10,
          "deveui": "0E7A5B118B3F102B",
          "appeui": "8FCDCEB406ADC006",
          "appkey": "C66598C4A8907C38790AEC2B4B5B2341",
          "nwkskey": "E2A3D6B2C3457899AABBCCDDEEFF0011",
          "appskey": "112233445566778899AABBCCDDEEFF00",
          "devaddr": "26011BA0"
        }
      })json";
    writeFileFS(loraConfigPath, defaultLora);
    Serial.println("✍️ Default lora.json created (Flash)");
  }

  // --- Load and apply ethernet config ---
  if (fileExistsFS(netConfigPath)) {
    String netJson = readFileFS(netConfigPath);
    StaticJsonDocument<2048> netDoc;
    if (!deserializeJson(netDoc, netJson)) {
      JsonObject obj = netDoc.as<JsonObject>();
      configureMACAddressFromJson(obj);
      configureEthernetFromJson(obj);
    }
  }

  // --- Load and apply lora config ---
  if (fileExistsFS(loraConfigPath)) {
    String loraJson = readFileFS(loraConfigPath);
    StaticJsonDocument<2048> loraDoc;
    DeserializationError err = deserializeJson(loraDoc, loraJson);
    if (!err) {
      configureLoRaFromJson(loraDoc.as<JsonObject>());
    } else {
      Serial.println("⚠️ Failed to parse lora.json");
      Serial.println(err.c_str());
    }
  } else {
    Serial.println("⚠️ No lora.json config found");
  }

  // --- Write default modbus.json if missing ---
  if (!fileExistsFS(configPath)) {
    String defaultModbus = R"json({
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
    writeFileFS(configPath, defaultModbus);
    Serial.println("✍️ Default modbus.json created (Flash)");
  }

  // --- Load Modbus requests ---
  if (fileExistsFS(configPath)) {
    String jsonStr = readFileFS(configPath);
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) {
      Serial.println("❌ JSON parse error");
      return;
    }

    if (doc.containsKey("interval")) {
      MODBUS_SCAN_INTERVAL = doc["interval"].as<unsigned long>();
      Serial.printf("⏱️ Modbus scan interval set to %lu ms\n", MODBUS_SCAN_INTERVAL);
    }

    JsonArray arr = doc["requests"].as<JsonArray>();
    requestCount = 0;

    for (JsonObject obj : arr) {
      if (!obj.containsKey("ip") || obj["ip"].size() != 4 ||
          !obj.containsKey("unitID") || !obj.containsKey("start") ||
          !obj.containsKey("count") || !obj.containsKey("function")) {
        Serial.println("⚠️ Skipping malformed request");
        continue;
      }

      int count = obj["count"];
      int function = obj["function"];
      if (count <= 0 || count > 125 || !(function >= 1 && function <= 4)) {
        Serial.println("⚠️ Invalid Modbus parameters");
        continue;
      }

      if (requestCount >= MAX_REQUESTS) break;
      ModbusRequest& req = requests[requestCount++];

      req.slaveIP = IPAddress(obj["ip"][0], obj["ip"][1], obj["ip"][2], obj["ip"][3]);
      req.unitID = obj["unitID"];
      req.startReg = obj["start"];
      req.numRegs = obj["count"];
      req.function = static_cast<ModbusFunction>(function);
      req.success = false;

      // Alarms
      req.alarmCount = 0;
      if (obj.containsKey("alarms")) {
        JsonArray alarmArr = obj["alarms"];
        for (JsonObject alarm : alarmArr) {
          if (req.alarmCount >= 4) break;
          if (!alarm.containsKey("index") || !alarm.containsKey("op") || !alarm.containsKey("threshold")) continue;

          AlarmCondition& a = req.alarms[req.alarmCount++];
          a.index = alarm["index"];
          a.op = alarm["op"].as<const char*>()[0];
          a.threshold = alarm["threshold"];
          a.active = false;
        }
        Serial.printf(" 🔔 %d alarms configured for request %d\n", req.alarmCount, requestCount - 1);
      }
    }

    Serial.printf("✅ Loaded %d Modbus requests from %s\n", requestCount, configPath);
  } else {
    Serial.printf("⚠️ No Modbus config file found at %s\n", configPath);
  }
}

void loadInputsConfig(const char* path) {
  if (!fileExistsFS(path)) {
    writeFileFS(path, R"json({
      "inputs": [
        { "pin": 14, "type": "digital", "alarm": { "active": true, "expected": 1 } },
        { "pin": 15, "type": "counter", "alarm": { "active": false } }
      ]
    })json");
    Serial.println("✍️ Default inputs.json created");
  }

  String content = readFileFS(path);
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, content);
  if (err) {
    Serial.println("❌ Failed to parse inputs.json");
    return;
  }

  JsonArray arr = doc["inputs"];
  for (int i = 0; i < 2 && i < arr.size(); i++) {
    auto obj = arr[i];
    inputConfigs[i].pin = obj["pin"];
    inputConfigs[i].type = strcmp(obj["type"], "counter") == 0 ? COUNTER : DIGITAL;
    inputConfigs[i].alarmActive = obj["alarm"]["active"];
    inputConfigs[i].alarmExpected = obj["alarm"]["expected"] | 0;
    pinMode(inputConfigs[i].pin, INPUT);
  }
}

void saveFrameCounter() {
  File file = LittleFS.open("/fcnt.txt", "w");
  if (file) {
    file.print(LMIC.seqnoUp);
    file.close();
  }
}

void loadFrameCounter() {
  File file = LittleFS.open("/fcnt.txt", "r");
  if (file) {
    LMIC.seqnoUp = file.parseInt();
    file.close();
    Serial.printf("🔁 Restored frame counter: %lu\n", LMIC.seqnoUp);
  }
}

