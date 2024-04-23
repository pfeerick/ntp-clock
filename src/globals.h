#pragma once

#define DEBUG true
#define DEBUG_OI Serial

#define OTA_HOSTNAME "NTP_Clock"
#define VERSION "0.6.0"
#define DEVICE_NAME "NTP Clock"

#include <Arduino.h>       // Arduino core functions
#include <TimeLib.h>       // Friendly time formatting and timekeeping
#include <debug-helper.h>  // Debug macros

inline constexpr uint32_t sleepTime = 50;  // Duration to sleep between loops
inline uint32_t uptime;  // Counting every second until 4294967295 = 130 year
inline uint32_t loop_load_avg;  // Indicative loop load average

inline bool restartDevice = false;    // Flag that device restart requested
inline constexpr int timeZone = 10;   // AEST
inline constexpr int BUTTON_PIN = 0;  // Connect button between GPIO0 and GND
inline constexpr uint32_t ntpUpdateInterval = 60 * 60 * 8;  // every eight hours
