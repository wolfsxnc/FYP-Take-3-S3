#include <SPI.h>
#include <Ethernet.h>
#include <ModbusEthernet.h>
#include "config.h"
#include "Modbus.h"

ModbusEthernet mb;

bool ethOK;
unsigned long lastModbusReadTime;

void initEthernet() {
    if (!enableEthernet) {
      Serial.println("🚫 Ethernet disabled via config — skipping setup");
      return;
    }
  
    pinMode(CS_W5500, OUTPUT);
    digitalWrite(CS_W5500, HIGH);
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
  
    Ethernet.init(CS_W5500);

     // Show configured IP settings
  Serial.println("🧾 Ethernet Config:");
  Serial.print("  MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", MAC_ADDR[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  Serial.print("  Static IP: ");     Serial.println(ETH_IP);
  Serial.print("  Gateway:   ");     Serial.println(ETH_GATEWAY);
  Serial.print("  Subnet:    ");     Serial.println(ETH_SUBNET);
  Serial.print("  DNS:       ");     Serial.println(ETH_DNS);
  Serial.print("  DHCP:      ");     Serial.println(useDHCP ? "Enabled" : "Disabled");

  
    // Check for physical link (W5500 only)
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("⚠️ Ethernet cable is not connected");
      ethOK = false;
    }
  
    if (useDHCP) {
      Serial.println("🌐 Attempting DHCP...");
      if (Ethernet.begin(MAC_ADDR) == 0) {
        Serial.println("❌ DHCP failed — falling back to static IP");
  
        Ethernet.begin(MAC_ADDR, ETH_IP, ETH_DNS, ETH_GATEWAY, ETH_SUBNET);
        if (Ethernet.localIP() == IPAddress(0, 0, 0, 0)) {
          Serial.println("❌ Static IP config also failed");
          ethOK = false;
        } else {
          Serial.println("🌐 Static IP configured (fallback)");
          ethOK = true;
        }
      } else {
        Serial.println("🌐 DHCP successful");
        ethOK = true;
      }
    } else {
      Ethernet.begin(MAC_ADDR, ETH_IP, ETH_DNS, ETH_GATEWAY, ETH_SUBNET);
      if (Ethernet.localIP() == IPAddress(0, 0, 0, 0)) {
        Serial.println("❌ Static IP config failed");
        ethOK = false;
      } else {
        Serial.println("🌐 Static IP configured");
        ethOK = true;
      }
    }
  
    if (ethOK) {
      printIP();
      mb.client();
    } else {
      Serial.println("🚫 Ethernet setup failed — continuing without Modbus");
    }
  }
  

void printIP() {
  Serial.print("🖧 IP: ");
  Serial.println(Ethernet.localIP());
}

void pollModbus() {
    if (!enableEthernet || !ethOK) {
      Serial.println("🚫 Modbus polling skipped — Ethernet is disabled");
      return;
    }
  
    for (int i = 0; i < requestCount; i++) {
      ModbusRequest& req = requests[i];
      req.success = false;
  
      if (!mb.isConnected(req.slaveIP)) {
        Serial.print("🔌 Modbus: not connected to ");
        Serial.println(req.slaveIP);
        mb.connect(req.slaveIP);
        continue;
      }
  
      Serial.print("🔗 Polling slave ");
      Serial.print(req.slaveIP);
      Serial.print(" @ unit ");
      Serial.println(req.unitID);
  
      bool ok = false;
      switch (req.function) {
        case READ_HREG:
          ok = mb.readHreg(req.slaveIP, req.startReg, req.result, req.numRegs, nullptr, req.unitID);
          break;
        case READ_IREG:
          ok = mb.readIreg(req.slaveIP, req.startReg, req.result, req.numRegs, nullptr, req.unitID);
          break;
        case READ_COILS: {
          bool bits[16];
          ok = mb.readCoil(req.slaveIP, req.startReg, bits, req.numRegs, nullptr, req.unitID);
          if (ok) {
            for (int j = 0; j < req.numRegs; j++) {
              req.result[j] = bits[j] ? 1 : 0;
            }
          }
          break;
        }
        case READ_DISCRETE_INPUTS: {
          bool bits[16];
          ok = mb.readIsts(req.slaveIP, req.startReg, bits, req.numRegs, nullptr, req.unitID);
          if (ok) {
            for (int j = 0; j < req.numRegs; j++) {
              req.result[j] = bits[j] ? 1 : 0;
            }
          }
          break;
        }
      }
  
      if (ok) {
        unsigned long start = millis();
        while (millis() - start < 50) mb.task();
        req.success = true;
  
        // === 🔔 Evaluate Alarms ===
        for (int a = 0; a < req.alarmCount; a++) {
          AlarmCondition& alarm = req.alarms[a];
  
          if (alarm.index >= req.numRegs) continue;
  
          uint16_t value = req.result[alarm.index];
          bool triggered = false;
  
          switch (alarm.op) {
            case '>': triggered = value > alarm.threshold; break;
            case '<': triggered = value < alarm.threshold; break;
            case '=': triggered = value == alarm.threshold; break;
          }
  
          // Edge-trigger: only fire once when condition becomes true
          if (triggered && !alarm.active) {
            alarm.active = true;
            alarm.pending = true;
            Serial.printf("🚨 Alarm triggered: Reg[%d] = %u %c %u\n", alarm.index, value, alarm.op, alarm.threshold);
            // TODO: mark for immediate uplink
          } else if (!triggered && alarm.active) {
            alarm.active = false;  // reset trigger
          }
        }
      }
    }
  }
  