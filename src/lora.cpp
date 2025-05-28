// ===== src/lora.cpp =====
#include <Arduino.h>
#include "config.h"
#include "lora.h"
#include "flashfs.h"

void os_getArtEui(u1_t* buf) { memcpy(buf, APPEUI, sizeof(APPEUI)); }
void os_getDevEui(u1_t* buf) { memcpy(buf, DEVEUI, sizeof(DEVEUI)); }
void os_getDevKey(u1_t* buf) { memcpy(buf, APPKEY, sizeof(APPKEY)); }


const lmic_pinmap lmic_pins = {
  .nss  = LORA_NSS_PIN,
  .rxtx = LMIC_UNUSED_PIN,
  .rst  = LORA_RST_PIN,
  .dio  = { LORA_DIO0_PIN, LORA_DIO1_PIN, LMIC_UNUSED_PIN }
};

volatile bool txComplete = false;

void resetLoRaChip() {
  digitalWrite(LORA_RST_PIN, LOW); delay(10);
  digitalWrite(LORA_RST_PIN, HIGH); delay(10);
}


void initLoRa() {
  pinMode(LORA_RST_PIN, OUTPUT);
  os_init_ex(&lmic_pins);
  LMIC_reset();
  applyLoRaConfig();

  if (JOIN_MODE_ABP) {
    LMIC_setSession(0x13, DEVADDR, NWKSKEY, APPSKEY);
    LMIC_setLinkCheckMode(0);  // disable MAC link check in ABP
    joined = true;
    Serial.println("🔐 ABP session initialized");
  } else {
    LMIC_startJoining();
    Serial.println("🔄 OTAA join started");
  }
}



void sendLoRaUplink() {
  if (LMIC.opmode & OP_TXRXPEND) return;

  uint8_t sf = getCurrentSF();
  uint16_t mtu = getMaxMTU(sf);

  uint8_t buffer[256];  // working chunk
  uint8_t index = 0;

  for (int i = 0; i < requestCount; i++) {
    uint8_t reqBuf[64];  // temporary buffer for one request
    uint8_t reqLen = 0;

    ModbusRequest& req = requests[i];

    reqBuf[reqLen++] = req.slaveIP[0];
    reqBuf[reqLen++] = req.slaveIP[1];
    reqBuf[reqLen++] = req.slaveIP[2];
    reqBuf[reqLen++] = req.slaveIP[3];
    reqBuf[reqLen++] = req.unitID;
    reqBuf[reqLen++] = highByte(req.startReg);
    reqBuf[reqLen++] = lowByte(req.startReg);
    reqBuf[reqLen++] = req.numRegs;
    reqBuf[reqLen++] = req.success ? 1 : 0;
    reqBuf[reqLen++] = static_cast<uint8_t>(req.function);

    if (!req.success) {
      uint8_t filler = (req.function <= 2) ? ceil(req.numRegs / 8.0) : req.numRegs * 2;
      while (filler--) reqBuf[reqLen++] = 0x00;
    } else if (req.function <= 2) {
      uint8_t bitPacked = 0;
      int bitIndex = 0;
      for (int r = 0; r < req.numRegs; r++) {
        if (req.result[r] & 0x01) bitPacked |= (1 << bitIndex);
        bitIndex++;
        if (bitIndex == 8 || r == req.numRegs - 1) {
          reqBuf[reqLen++] = bitPacked;
          bitPacked = 0;
          bitIndex = 0;
        }
      }
    } else {
      for (int r = 0; r < req.numRegs; r++) {
        reqBuf[reqLen++] = highByte(req.result[r]);
        reqBuf[reqLen++] = lowByte(req.result[r]);
      }
    }

    // Check if this request fits
    if (index + reqLen > mtu) {
      sendLoRaPayloadChunk(buffer, index);
      index = 0;
    }

    memcpy(&buffer[index], reqBuf, reqLen);
    index += reqLen;
  }

  // Send digital input section
  uint8_t inputSection[16];
  uint8_t inputLen = 0;

  inputSection[inputLen++] = 0xFF;
  inputSection[inputLen++] = 2;

  for (int i = 0; i < 2; i++) {
    inputSection[inputLen++] = i;
    inputSection[inputLen++] = inputConfigs[i].type;
    if (inputConfigs[i].type == COUNTER) {
      inputSection[inputLen++] = highByte(inputConfigs[i].counterValue);
      inputSection[inputLen++] = lowByte(inputConfigs[i].counterValue);
    } else {
      inputSection[inputLen++] = 0x00;
      inputSection[inputLen++] = inputConfigs[i].lastState ? 1 : 0;
    }
  }

  if (index + inputLen > mtu) {
    sendLoRaPayloadChunk(buffer, index);
    index = 0;
  }

  memcpy(&buffer[index], inputSection, inputLen);
  index += inputLen;

  // Final packet
  if (index > 0) {
    sendLoRaPayloadChunk(buffer, index);
  }

  uplinkCount++;
  resetCounters();
}

  void applyLoRaConfig() {
    LMIC_setAdrMode(LORA_ADR);
    LMIC_selectSubBand(LORA_SUBBAND);
  
    if (!LORA_ADR) {
      switch (LORA_SF) {
        case 7:  LMIC_setDrTxpow(DR_SF7, 14); break;
        case 8:  LMIC_setDrTxpow(DR_SF8, 14); break;
        case 9:  LMIC_setDrTxpow(DR_SF9, 14); break;
        case 10: LMIC_setDrTxpow(DR_SF10, 14); break;
        case 11: LMIC_setDrTxpow(DR_SF11, 14); break;
        case 12: LMIC_setDrTxpow(DR_SF12, 14); break;
        default:
          Serial.println("⚠️ Invalid SF in config — defaulting to SF7");
          LMIC_setDrTxpow(DR_SF7, 14);
      }
    }
  }

  void checkAlarmUplink() {
    for (int i = 0; i < requestCount; i++) {
      ModbusRequest& req = requests[i];
  
      for (int a = 0; a < req.alarmCount; a++) {
        AlarmCondition& alarm = req.alarms[a];
        if (alarm.pending) {
          sendAlarmUplink(req, alarm, req.result[alarm.index]);
          alarm.pending = false;  // ✅ clear it after send
          return;  // ✅ only send one per loop
        }
      }
    }
  }

  void sendAlarmUplink(const ModbusRequest& req, const AlarmCondition& alarm, uint16_t value) {
    if (LMIC.opmode & OP_TXRXPEND) return;
  
    uint8_t payload[12];  // 4 IP, 1 unit, 2 reg, 1 op, 2 threshold, 2 value
    uint8_t i = 0;
  
    for (int j = 0; j < 4; j++) payload[i++] = req.slaveIP[j];
    payload[i++] = req.unitID;
    payload[i++] = highByte(req.startReg + alarm.index);
    payload[i++] = lowByte(req.startReg + alarm.index);
    payload[i++] = alarm.op;
    payload[i++] = highByte(alarm.threshold);
    payload[i++] = lowByte(alarm.threshold);
    payload[i++] = highByte(value);
    payload[i++] = lowByte(value);
  
    txComplete = false;
    LMIC_setTxData2(2, payload, i, 1);  // ⚠️ fPort = 2 for alarms
  
    while (!txComplete) os_runloop_once();
    LMIC_clrTxData();
  
    Serial.println("📡 Alarm uplink sent");
  }

  void sendAlarmUplink(uint8_t inputIndex, uint8_t expected, uint8_t actual) {
    if (LMIC.opmode & OP_TXRXPEND) return;

    uint8_t payload[5];  // 1 input index, 1 expected, 1 actual, + 2 reserved for future
    payload[0] = 0xFF;   // Input alarm flag (distinct from IP start of Modbus)
    payload[1] = inputIndex;
    payload[2] = expected;
    payload[3] = actual;
    payload[4] = 0x00;   // Reserved or checksum, optional

    txComplete = false;
    LMIC_setTxData2(2, payload, 5, 1);  // fPort = 2 = alarm
    
    while (!txComplete) os_runloop_once();
    LMIC_clrTxData();

    Serial.printf("📡 Digital Input Alarm %d: expected %d, got %d\n", inputIndex, expected, actual);
}

  
  
  

  
  
void onEvent(ev_t ev) {
  switch (ev) {
    case EV_TXSTART:
      Serial.printf("📡 TX Start - freq: %lu Hz, sf: SF%d\n", LMIC.freq, LMIC.datarate);
      break;
    case EV_JOINING:  Serial.println("EV_JOINING"); break;
    case EV_JOINED:   Serial.println("✅ EV_JOINED"); joined = true; break;
    case EV_TXCOMPLETE:
      //resetLoRaChip(); 
      txComplete = true;
      Serial.println("EV_TXCOMPLETE");
      saveFrameCounter();
      if (LMIC.txrxFlags & TXRX_ACK) {
        Serial.println("✅ Server ACK received");
      } else {
        Serial.println("❌ No ACK received");
      }
      break;
    default: Serial.println((unsigned)ev); break;
  }
}


uint8_t getCurrentSF() {
  // LMIC.datarate values: DR_0 = SF12 ... DR_5 = SF7 (for AU915/US915)
  return 12 - LMIC.datarate;  // DR_0 = 0 -> SF12, DR_5 = 5 -> SF7
}

uint16_t getMaxMTU(uint8_t sf) {
  switch (sf) {
    case 7:
    case 8: return 242;
    case 9: return 115;
    case 10:
    case 11:
    case 12: return 51;
    default: return 51;
  }
}

void sendLoRaPayloadChunk(uint8_t* payload, uint8_t len) {
  txComplete = false;
  LMIC_setTxData2(1, payload, len, 1);

  unsigned long start = millis();
  while (!txComplete && millis() - start < 3000) {
    os_runloop_once();
    yield();  // Allow Serial input / shell / background tasks
  }

  if (!txComplete) {
    Serial.println("⚠️ TX timeout — no confirmation from LMIC");
  }

  LMIC_clrTxData();
}


