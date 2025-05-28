#pragma once
#include <lmic.h>
#include <hal/hal.h>
#include "inputs.h"

void initLoRa();
void resetLoRaChip();
void sendLoRaUplink();
void onEvent(ev_t ev);
void applyLoRaConfig();
void checkAlarmUplink();
void sendAlarmUplink(const ModbusRequest& req, const AlarmCondition& alarm, uint16_t value); //For Modbus
void sendAlarmUplink(uint8_t inputIndex, uint8_t expected, uint8_t actual); //For Digital inputs

uint8_t getCurrentSF();
uint16_t getMaxMTU(uint8_t sf);
void sendLoRaPayloadChunk(uint8_t* payload, uint8_t len);

