#pragma once
#include <Arduino.h>

#define HISTORY_SIZE 10

extern String commandHistory[HISTORY_SIZE];
extern int historyIndex;
extern int currentHistoryPos;

void handleSerialCommand();
void executeCommand(const String& input);
void writeDefaultConfigs();
void editFile(const char* path);
String getUserLine(const String& prompt = "");
