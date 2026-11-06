#pragma once
#include <Arduino.h>
// Keep this version in sync with the firmware release tag.
#ifndef EMLALOCK_VERSION
#define EMLALOCK_VERSION "1.0.8"
#endif
void initUpdateManager();
void handleUpdateCheck();
void handleUpdateStart();
const char* firmwareVersion();
