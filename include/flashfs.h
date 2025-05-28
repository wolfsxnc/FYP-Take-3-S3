#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

extern IPAddress ETH_IP;
extern IPAddress ETH_GATEWAY;
extern IPAddress ETH_SUBNET;
extern IPAddress ETH_DNS;
extern bool useDHCP;
extern uint8_t MAC_ADDR[6];
extern bool enableEthernet;

extern unsigned long LORA_UPLINK_INTERVAL;
extern uint8_t LORA_SUBBAND;
extern bool LORA_ADR;
extern uint8_t LORA_SF;


extern uint8_t APPKEY[16];
extern uint8_t APPEUI[8];
extern uint8_t DEVEUI[8];

bool initFlashFS();
bool fileExistsFS(const char* path);
String readFileFS(const char* path);
bool writeFileFS(const char* path, const String& content);
bool appendToFileFS(const char* path, const String& content);

void loadModbusConfigFromFlash(const char* configPath = "/modbus.json");
void writeDefaultConfigs();

void loadInputsConfig(const char* path = "/inputs.json");
void loadFrameCounter();
void saveFrameCounter();