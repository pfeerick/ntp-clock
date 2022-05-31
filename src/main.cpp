/**
 * This file is part of the NTP LED Matrix Clock.
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
 * GPIOs usage:
 *
 * Wemos D1 Mini:

 *      /----------------------\
 *      |     |?|?|_|?|_|?|    |
 *      |     | |         |    |
 *  [ ] | RST          LED| TX | [ ]
 *  [ ] | A0  |???????????| RX | [ ]
 *  [M] | D0  |           | D1 | [G]
 *  [M] | D5  |           | D2 | [G]
 *  [ ] | D6  |  ESP8266  | D3 | [B]
 *  [M] | D7  |           | D4 | [ ]
 *  [ ] | D8  |___________|  G | [ ]
 *  [ ] | 3V3               5V | [ ]
 *       \                     |
 *       [| RST        D1 Mini |
 *        |_____|  USB  |______|
 *
 * [M] MAX72xx Panel
 *     IO16 - D0 - CS
 *     IO14 - D5 - CLK
 *     IO13 - D7 - MOSI
 *
 * [G] GY-521 / MPU-6050
 *     IO5  - D1 - SDA/XDA
 *     IO4  - D2 - SCL
 *
 * [B] Button
 *     IO0  - D3  - Button -> GND
 *
 * Digistump Oak:
 *           Enable    |      VIN        X - Provide 5v to rest of circuit
 *           Reset     |      GND        X
 *     P11 / A0  / 17  |  4 / P5         A
 * D  Wake / P10 / 16  |  1 / P4 / TX    TX - don't hold low at boot
 * D  SCLK / P9  / 14  |  3 / P3 / RX    RX
 *    MISO / P8  / 12  |  0 / P2 / SCL   B  - don't hold low at boot
 * D  MOSI / P7  / 13  |  5 / P1 / LED   LED
 *      SS / P6  / 15  |  2 / P0 / SDA   A - don't hold low at boot
 *           GND       |      VCC        X - 3v3 - level shifter + AXDL
 *
 * D - Display
 * B - Button
 * A - Accelerometer
*/

#include <globals.h>          // Global libraries and variables
#include <displayHelper.h>    // Display helper functions
#include <elapsedMillis.h>    // Non-blocking delays / event timers
#include <sensorHelper.h>     // Sensor helper functions
#include <sleepHelper.h>      // Sleep helper functions
#include <webserverHelper.h>  // Web server helper functions
#include <wifiHelper.h>       // WiFi helper functions

elapsedMillis orientationCheck;
constexpr uint16_t orientationCheckInterval = 500;
constexpr uint8_t delayAfterRestart = 100;

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
  wifi::setupNTP(28800);

  webserver::setupHTTP();

  sensor::button.begin();

  display::printMsg("Ready");
}

time_t prevDisplay = 0;  // when the digital clock was displayed

void loop()
{
  uint32_t loopStart = millis();
  sleep::updateUptime();

  if (WiFi.status() == WL_CONNECTED) {
    wifi::otaLoopTask();
    webserver::loopTask();
  }

  sensor::gyroUpdate();
  if (orientationCheck > orientationCheckInterval) {
    orientationCheck = 0;  // reset counter
    display::setDisplayOrientation(sensor::gyroGetValue(sensor::Y_AXIS, false));
  }

  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) {  // update the display only if time has changed
      prevDisplay = now();
      display::digitalClockDisplay();
    }
  }

  if (sensor::button.pressed()) {
    DebugPrintln("Button Pressed!");

    const String tape = "Let go of me!";
    constexpr int wait = 30;  // In milliseconds - was 20

    constexpr int spacer = 1;
    constexpr int width = 5 + spacer;  // The font width is 5 pixels

    for (unsigned int i = 0;
         i < width * tape.length() + display::matrix.width() - 1 - spacer;
         i++) {
      display::matrix.fillScreen(LOW);

      unsigned int letter = i / width;
      int x = (display::matrix.width() - 1) - i % width;
      int y = (display::matrix.height() - 8) / 2;  // center the text vertically

      while (x + width - spacer >= 0 && letter >= 0) {
        if (letter < tape.length()) {
          display::matrix.drawChar(x, y, tape[letter], HIGH, LOW, 1);
        }

        letter--;
        x -= width;
      }

      display::matrix.write();  // Send bitmap to display
      delay(wait);
    }
  }

  // Restart command received
  if (restartDevice == true) {
    ESP.restart();
    delay(delayAfterRestart);
  }

  // Restart if WiFi down for more than 5 minutes
  if ((WiFi.status() != WL_CONNECTED) && (millis() > 600e3)) {
    ESP.restart();
    delay(delayAfterRestart);
  }

  sleep::dynamicSleep(loopStart);
}
