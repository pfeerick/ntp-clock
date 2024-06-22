/**
 * This file is part of the NTP LED Matrix Clock Project
 * Copyright (c) Peter Feerick
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
*/

#include <displayHelper.h>    // Display helper functions
#include <elapsedMillis.h>    // Non-blocking delays / event timers
#include <globals.h>          // Global libraries and variables
#include <sensorHelper.h>     // Sensor helper functions
#include <sleepHelper.h>      // Sleep helper functions
#include <webserverHelper.h>  // Web server helper functions
#include <wifiHelper.h>       // WiFi helper functions

elapsedMillis orientationCheck;
constexpr uint16_t orientationCheckInterval = 500;
constexpr uint8_t delayAfterRestart = 100;
constexpr uint16_t wifiDisconnectDelayBeforeRestart = 60 * 5;
time_t prevDisplay = 0;  // time when the digital clock was displayed last

void setup()
{
  DebugBegin(115200);
  DebugInfo();

  sensor::useIMU = false;
  sensor::initGyro();

  display::setDisplayOrientation(sensor::gyroGetValue(sensor::Y_AXIS, true));
  display::setup(1);

  wifi::setupWifi();
  wifi::setupUDP();
  wifi::setupOTA();
  wifi::setupNTP(ntpUpdateInterval); // every eight hours

  webserver::setupHTTP();

  sensor::button.begin();

  display::printMsg("Ready");
}

void loop()
{
  uint32_t loopStart = millis();
  sleep::updateUptime();

  wifi::WifiCheckState();

  if (WiFi.status() == WL_CONNECTED) {
    wifi::otaLoopTask();
    webserver::loopTask();
  }

  // check if gyro orientation has changed and rotate display accordingly
  sensor::gyroUpdate();
  if (orientationCheck > orientationCheckInterval) {
    orientationCheck = 0;  // reset counter
    display::setDisplayOrientation(sensor::gyroGetValue(sensor::Y_AXIS, false));
  }

  // update display if time set and has changed
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) {
      prevDisplay = now();
      display::digitalClockDisplay();
    }
  }

  // message / action if button pressed
  if (sensor::button.pressed()) {
    DebugPrintln("Button Pressed!");
    display::scrollingText("Let go of me!", 30);
  }

  // Restart command received or WiFi down for more than specified time
  if (restartDevice == true ||
      ((WiFi.status() != WL_CONNECTED) &&
       (wifi::downtime >= wifiDisconnectDelayBeforeRestart))) {
    ESP.restart();
    delay(delayAfterRestart);
  }

  sleep::dynamicSleep(loopStart);
}
