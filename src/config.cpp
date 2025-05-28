// ===== src/config.cpp =====
#include "config.h"
#include "IPAddress.h"

byte MAC_ADDR[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE };

//byte MAC_ADDR[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

unsigned long lastModbusPoll = 0;
bool joined = false;
uint16_t uplinkCount = 0;

unsigned long MODBUS_SCAN_INTERVAL = 5000;


// Format: { IPAddress(a, b, c, d), unitID, startRegister, numRegisters, {0}, successFlag, functionCode}
ModbusRequest requests[MAX_REQUESTS] = {
    ModbusRequest(IPAddress(192, 168, 0, 187), 1, 0, 4, READ_HREG),
    ModbusRequest(IPAddress(192, 168, 0, 187), 1, 4100, 8, READ_COILS),
    ModbusRequest(IPAddress(192, 168, 0, 187), 1, 1100, 2, READ_IREG),
    ModbusRequest(IPAddress(192, 168, 0, 187), 1, 3100, 6, READ_DISCRETE_INPUTS)
  };
  
  
int requestCount = 4;

uint8_t DEVEUI[8]  = { 0x0E,0x7A,0x5B,0x11,0x8B,0x3F,0x10,0x2B };
uint8_t APPEUI[8]  = { 0x8F,0xCD,0xCE,0xB4,0x06,0xAD,0xBC,0x06 };
uint8_t APPKEY[16] = {
  0xC6,0x65,0x98,0xC4,0xA8,0x90,0x7C,0x38,
  0x79,0x0A,0xEC,0x2B,0x4B,0x5B,0x23,0x41
};

// Define the actual global storage
IPAddress ETH_IP(192, 168, 0, 100);
IPAddress ETH_GATEWAY(192, 168, 0, 1);
IPAddress ETH_SUBNET(255, 255, 255, 0);
IPAddress ETH_DNS(8, 8, 8, 8);
bool useDHCP = true;
bool enableEthernet = true;  // default: enabled


// LoRa configuration defaults
unsigned long LORA_UPLINK_INTERVAL = 10000;  // ms
uint8_t LORA_SUBBAND = 4;                    // AU915 sub-band 4
bool LORA_ADR = true;                        // Adaptive Data Rate
uint8_t LORA_SF = 7;  // default to SF7
unsigned long lastUplink = -LORA_UPLINK_INTERVAL;  // triggers immediately

// ----- Join Mode -----
bool JOIN_MODE_ABP = false;


// ----- ABP -----
uint8_t NWKSKEY[16] = { 0 };
uint8_t APPSKEY[16] = { 0 };
uint32_t DEVADDR = 0;

