#pragma once
#include <IPAddress.h>
#include <Arduino.h>
#include "inputs.h"

#define SCK_PIN      36
#define MISO_PIN     37
#define MOSI_PIN     35
#define CS_W5500     10
#define LORA_RST_PIN 6

#define LORA_NSS_PIN   5
#define LORA_DIO0_PIN 11
#define LORA_DIO1_PIN 8

#define MAX_REQUESTS 16
#define MAX_ALARMS_PER_REQUEST 4

enum ModbusFunction {
  READ_HREG = 3,
  READ_IREG = 4,
  READ_COILS = 1,
  READ_DISCRETE_INPUTS = 2
};

struct AlarmCondition {
    uint8_t index;        // result[] index
    char op;              // '>', '<', '='
    uint16_t threshold;
    bool active = false;  // track edge triggering
    bool pending;
  };
  
  
  struct ModbusRequest {
    IPAddress slaveIP;
    uint8_t unitID;
    uint16_t startReg;
    uint8_t numRegs;
    uint16_t result[16];
    bool success;
    ModbusFunction function;
    AlarmCondition alarms[4];
    uint8_t alarmCount;
  
    ModbusRequest(
      IPAddress ip = IPAddress(0, 0, 0, 0),
      uint8_t unit = 1,
      uint16_t start = 0,
      uint8_t count = 1,
      ModbusFunction func = READ_HREG
    ) :
      slaveIP(ip),
      unitID(unit),
      startReg(start),
      numRegs(count),
      success(false),
      function(func),
      alarmCount(0)
    {
      memset(result, 0, sizeof(result));
      memset(alarms, 0, sizeof(alarms));
    }
  };
  


extern ModbusRequest requests[MAX_REQUESTS];
extern int requestCount;

extern IPAddress ETH_IP, ETH_GATEWAY, ETH_SUBNET, ETH_DNS;
extern bool useDHCP;
extern bool enableEthernet;



extern unsigned long MODBUS_SCAN_INTERVAL;
extern unsigned long LORA_UPLINK_INTERVAL;


extern unsigned long lastModbusPoll;
extern unsigned long lastUplink;
extern bool joined;
extern uint16_t uplinkCount;

// ----- Join Mode -----
extern bool JOIN_MODE_ABP;

// ----- OTAA -----
extern uint8_t DEVEUI[8];
extern uint8_t APPEUI[8];
extern uint8_t APPKEY[16];

// ----- ABP -----
extern uint8_t NWKSKEY[16];
extern uint8_t APPSKEY[16];
extern uint32_t DEVADDR;


extern uint8_t MAC_ADDR[6];

extern uint8_t LORA_SF;  // e.g., 7–12 for SF7 to SF12
extern bool LORA_ADR; // Yes please
extern uint8_t LORA_SUBBAND; // 4 for australia

