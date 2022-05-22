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

#define DEBUG true
#define DEBUG_OI Serial

#define OTA_HOSTNAME "NTP_Clock"
#define VERSION "0.5.0"
#define DEVICE_NAME "NTP Clock"

#include <Arduino.h>            // Arduino core functions
#include <ESP8266WiFi.h>        // ESP8266 Core WiFi Library
#include <DNSServer.h>          // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>   // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>        // WiFi Configuration Portal
#include <ESP8266mDNS.h>        // Multicast DNS (for OTA)
#include <WiFiUdp.h>            // UDP support (for NTP)
#include <ArduinoOTA.h>         // OTA programming support
#include <TimeLib.h>            // Friendly time formatting and timekeeping
#include <SPI.h>                // SPI device support
#include <Adafruit_I2CDevice.h> // Adafruit I2C device support
#include <Adafruit_GFX.h>       // Adafruit GFX routines
#include <Max72xxPanel.h>       // MAX72xx multiplexer
#include <Wire.h>               // I2C device support
#include <MPU6050_tockn.h>      // MPU6050 Gyro + Accelometer
#include <Button.h>             // Simple button library
#include <elapsedMillis.h>      // Non-blocking delays / event timers
#include <debug-helper.h>       // Debug macros

#include "webpages.h"           // Web page source code

/************************* NTP SETUP ****************************************/
static const char ntpServerName[] = "au.pool.ntp.org";
const int timeZone = 10; // AEST

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
unsigned int localPort = 2390; // local port to listen for UDP packets

/************************* LED MATRIX SETUP *********************************/
int pinCS = 16;                     // Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
int numberOfHorizontalDisplays = 4; // Adjust this to your setup
int numberOfVerticalDisplays = 1;

Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

bool useIMU = true;
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
double pitch, roll, yaw;

bool restartDevice = false;

Button button(0); // Connect your button between GPIO0 and GND
MPU6050 mpu6050(Wire);
elapsedMillis orientationCheck;
const unsigned int orientationCheckInterval = 500;

ESP8266WebServer webserver(80);
WiFiClient espClient;

uint32_t sleepTime = 50; // Duration to sleep between loops
uint32_t uptime;         // Counting every second until 4294967295 = 130 year
uint32_t loop_load_avg;  // Indicative loop load average

/************************* FUNCTION PROTOTYPES ******************************/
time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);
void setDisplayOrientation();
void setupOTA();
void setupWifi();
void configModeCallback(WiFiManager *myWiFiManager);

void http_reset();
void http_infoPage();
void http_indexPage();
void notFound();

inline int32_t TimeDifference(uint32_t prev, uint32_t next);
int32_t TimePassedSince(uint32_t timestamp);
bool TimeReached(uint32_t timer);
void SetNextTimeInterval(uint32_t &timer, const uint32_t step);
void SleepDelay(uint32_t mseconds);

void setup()
{
  DebugBegin(115200);
  DebugInfo();

// Initalise GY-521 / MPU-6050
#if defined ARDUINO_ESP8266_WEMOS_OAK
  Wire.begin(P0, P5); //SDA, SCL
#elif defined ARDUINO_ESP8266_WEMOS_D1MINI
  Wire.begin(D2, D1); //SDA, SCL
#endif

  mpu6050.begin();
  //mpu6050.calcGyroOffsets(true); //3 second delay

  //0 low, 15 high
  matrix.setIntensity(1); // Set brightness between 0 and 15

  setDisplayOrientation();

  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.setTextColor(HIGH);

  setupWifi();

  // enable light modem sleep
  //wifi_set_sleep_type(LIGHT_SLEEP_T);
  //WiFi.setSleepMode(WIFI_LIGHT_SLEEP);

  setupOTA();

  DebugPrintln("*OTA: Ready");

  udp.begin(localPort);
  DebugPrint("*UDP: Running on local port ");
  DebugPrintln(udp.localPort());

  webserver.on("/", http_indexPage);
  webserver.on("/restart", http_reset);
  webserver.on("/info", http_infoPage);
  webserver.onNotFound(notFound);
  webserver.begin();

  matrix.fillScreen(LOW); //Empty the screen
  matrix.setCursor(0, 0); //Move the cursor to the end of the screen
  matrix.print("Ready");
  matrix.write();

  setSyncProvider(getNtpTime);
  setSyncInterval(28800); //every eight hours

  button.begin();
}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{
  uint32_t my_sleep = millis();

  static uint32_t state_second = 0;                // State second timer
  if (TimeReached(state_second)) {
    SetNextTimeInterval(state_second, 1000);
    uptime++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    // Allow MDNS discovery
    MDNS.update();

    // Poll / handle OTA programming
    ArduinoOTA.handle();

    // Web Server
    webserver.handleClient();
  }

  mpu6050.update();

  if (orientationCheck > orientationCheckInterval)
  {
    orientationCheck = 0; //reset counter
    setDisplayOrientation();
  }

  if (timeStatus() != timeNotSet)
  {
    if (now() != prevDisplay)
    { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
    }
  }

  if (button.pressed())
  {
    DebugPrintln("Button Pressed!");

    String tape = "Let go of me!";
    int wait = 30; // In milliseconds - was 20

    int spacer = 1;
    int width = 5 + spacer; // The font width is 5 pixels

    for (int i = 0; i < width * tape.length() + matrix.width() - 1 - spacer; i++)
    {
      matrix.fillScreen(LOW);

      int letter = i / width;
      int x = (matrix.width() - 1) - i % width;
      int y = (matrix.height() - 8) / 2; // center the text vertically

      while (x + width - spacer >= 0 && letter >= 0)
      {
        if (letter < tape.length())
        {
          matrix.drawChar(x, y, tape[letter], HIGH, LOW, 1);
        }

        letter--;
        x -= width;
      }

      matrix.write(); // Send bitmap to display
      delay(wait);
    }
  }

  // Restart command received
  if (restartDevice == true)
  {
    ESP.restart();
    delay(100);
  }

  // Restart if WiFi down for more than 5 minutes
  if ((WiFi.status() != WL_CONNECTED) && (millis() > 600e3))
  {
    ESP.restart();
    delay(100);
  }

  // Dynamic sleep
  uint32_t my_activity = millis() - my_sleep; // Time this loop has taken in milliseconds
  if (my_activity < sleepTime)
  {
    SleepDelay(sleepTime - my_activity); // Provide time for background tasks like wifi
  }
  else
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      SleepDelay(my_activity / 2); // If wifi down then force loop delay to 1/2 of my_activity period
    }
  }

  // Calculate loop load_avg
  if (!my_activity)
  {
    my_activity++;
  } // We cannot divide by 0
  uint32_t loop_delay = sleepTime;
  if (!loop_delay)
  {
    loop_delay++;
  }                                              // We cannot divide by 0
  uint32_t loops_per_second = 1000 / loop_delay; // We need to keep track of this many loops per second
  uint32_t this_cycle_ratio = 100 * my_activity / loop_delay;
  loop_load_avg = loop_load_avg - (loop_load_avg / loops_per_second) + (this_cycle_ratio / loops_per_second); // Take away one loop average away and add the new one

  // delay(10);
}

void digitalClockDisplay()
{
  matrix.fillScreen(LOW); //Empty the screen
  matrix.setCursor(0, 0); //Move the cursor to the end of the screen
  if (hourFormat12() < 10)
    matrix.print(" ");

  matrix.print(hourFormat12()); //Write the time

  if ((second() % 2) != 0)
  {
    matrix.drawPixel(12, 2, HIGH);
    matrix.drawPixel(12, 4, HIGH);
  }

  matrix.setCursor(14, 0); //Move the cursor to the end of the screen
  if (minute() < 10)
  {
    matrix.print("0");
  }
  matrix.print(minute());

  matrix.setCursor(27, 0);
  if (isAM())
  {
    // draw an A
    matrix.drawPixel(28, 2, HIGH); // *
    matrix.drawPixel(28, 3, HIGH); // *
    matrix.drawPixel(28, 4, HIGH); // *
    matrix.drawPixel(28, 5, HIGH); // *
    matrix.drawPixel(28, 6, HIGH); // *

    matrix.drawPixel(29, 1, HIGH); //**
    matrix.drawPixel(29, 4, HIGH); //**

    matrix.drawPixel(30, 1, HIGH); //***
    matrix.drawPixel(30, 4, HIGH); //***

    matrix.drawPixel(31, 2, HIGH); // *
    matrix.drawPixel(31, 3, HIGH); // *
    matrix.drawPixel(31, 4, HIGH); // *
    matrix.drawPixel(31, 5, HIGH); // *
    matrix.drawPixel(31, 6, HIGH); // *
  }
  else
  {
    // draw a P
    matrix.drawPixel(28, 1, HIGH); // *
    matrix.drawPixel(28, 2, HIGH); // *
    matrix.drawPixel(28, 3, HIGH); // *
    matrix.drawPixel(28, 4, HIGH); // *
    matrix.drawPixel(28, 5, HIGH); // *
    matrix.drawPixel(28, 6, HIGH); // *

    matrix.drawPixel(29, 1, HIGH); //**
    matrix.drawPixel(29, 4, HIGH); //**

    matrix.drawPixel(30, 1, HIGH); //***
    matrix.drawPixel(30, 4, HIGH); //***

    matrix.drawPixel(31, 2, HIGH); //   *
    matrix.drawPixel(31, 3, HIGH); //   *
  }

  matrix.write();

  // print time to debug also
  DebugPrint(hour());
  printDigits(minute());
  printDigits(second());
  DebugPrint(" ");
  DebugPrint(day());
  DebugPrint(".");
  DebugPrint(month());
  DebugPrint(".");
  DebugPrint(year());
  DebugPrintln();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  DebugPrint(":");
  if (digits < 10)
    DebugPrint('0');
  DebugPrint(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48;     // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (udp.parsePacket() > 0)
    ; // discard any previously received packets
  DebugPrintln("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  DebugPrint(ntpServerName);
  DebugPrint(": ");
  DebugPrintln(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500)
  {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
      DebugPrintln("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  DebugPrintln("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  DebugPrintln("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void setDisplayOrientation()
{
  if (useIMU)
  {
    for (int i = 0; i < 10; i++)
    {
      mpu6050.update();
      delay(5);
    }

    // float X = mpu6050.getAngleX();
    float Y = mpu6050.getAngleY();
    // float Z = mpu6050.getAngleZ();

    DebugPrint("angleY : ");
    DebugPrintln(Y);

    if (Y >= 40)
    {
      DebugPrintln("Display up");

      //right way up
      matrix.setRotation(0, 1); //you may have to change this section depending on your LED matrix setup
      matrix.setRotation(1, 1);
      matrix.setRotation(2, 1);
      matrix.setRotation(3, 1);

      //left to right
      matrix.setPosition(0, 0, 0); // The first display is at <0, 0>
      matrix.setPosition(1, 1, 0); // The second display is at <1, 0>
      matrix.setPosition(2, 2, 0); // The third display is at <2, 0>
      matrix.setPosition(3, 3, 0); // And the last display is at <3, 0>
    }
    else if (Y <= -40)
    {
      DebugPrintln("Display down");

      //upside down
      matrix.setRotation(0, 3); //you may have to change this section depending on your LED matrix setup
      matrix.setRotation(1, 3);
      matrix.setRotation(2, 3);
      matrix.setRotation(3, 3);

      //now right to left
      matrix.setPosition(0, 3, 0); // The first display is at <3, 0>
      matrix.setPosition(1, 2, 0); // The second display is at <2, 0>
      matrix.setPosition(2, 1, 0); // The third display is at <1, 0>
      matrix.setPosition(3, 0, 0); // And the last display is at <0, 0>
    }
    else
    {
      DebugPrintln("Display don't know");

      //right way up
      matrix.setRotation(0, 1); //you may have to change this section depending on your LED matrix setup
      matrix.setRotation(1, 1);
      matrix.setRotation(2, 1);
      matrix.setRotation(3, 1);

      //left to right
      matrix.setPosition(0, 0, 0); // The first display is at <0, 0>
      matrix.setPosition(1, 1, 0); // The second display is at <1, 0>
      matrix.setPosition(2, 2, 0); // The third display is at <2, 0>
      matrix.setPosition(3, 3, 0); // And the last display is at <3, 0>
    }
  }
  else
  {
    matrix.setRotation(0, 1); //you may have to change this section depending on your LED matrix setup
    matrix.setRotation(1, 1);
    matrix.setRotation(2, 1);
    matrix.setRotation(3, 1);
  }
}

void setupOTA()
{
  /// OTA Update init
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  ArduinoOTA.onStart([]() {
    DebugPrintln("OTA Programming Start");
    matrix.fillScreen(LOW); //Empty the screen
    matrix.setCursor(0, 0); //Move the cursor to the end of the screen
    matrix.print("OTA");
    matrix.write();
  });

  ArduinoOTA.onEnd([]() {
    DebugPrintln("\nOTA Programming End");
    matrix.fillScreen(LOW); //Empty the screen
    matrix.setCursor(0, 0); //Move the cursor to the end of the screen
    matrix.print("DONE!");
    matrix.write();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DebugPrintf("Progress: %u%%\r", (progress / (total / 100)));

    matrix.fillScreen(LOW); //Empty the screen
    matrix.setCursor(0, 0); //Move the cursor to the end of the screen
    matrix.print("P");

    //narrow space colon
    matrix.drawPixel(7, 2, HIGH);
    matrix.drawPixel(7, 4, HIGH);

    matrix.setCursor(9, 0);
    if ((progress / (total / 100)) < 10)
    {
      matrix.print("0");
    }
    matrix.print((progress / (total / 100)));
    matrix.print("%");
    matrix.write();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    DebugPrintf("Error[%u]: ", error);

    if (error == OTA_AUTH_ERROR)
      DebugPrintln("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      DebugPrintln("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      DebugPrintln("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      DebugPrintln("Receive Failed");
    else if (error == OTA_END_ERROR)
      DebugPrintln("End Failed");

    matrix.fillScreen(LOW); //Empty the screen
    matrix.setTextSize(1);
    matrix.setCursor(0, 0); //Move the cursor to the end of the screen
    matrix.print("OTA ER");
    matrix.write();
  });

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.begin();
}

void setupWifi()
{
  WiFiManager wifiManager;

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setConfigPortalTimeout(300); // 5 minute timout

#ifndef DEBUG
  wifiManager.setDebugOutput(false);
#endif

  matrix.fillScreen(LOW); //Empty the screen
  matrix.setCursor(0, 0); //Move the cursor to the end of the screen
  matrix.print("WiFi");
  matrix.write();

  if (!wifiManager.autoConnect(OTA_HOSTNAME))
  {
    DebugPrintln("Failed to connect and hit timeout");
    //reset and try again
    ESP.reset();
    delay(1000);
  }
}

void configModeCallback(WiFiManager *myWiFiManager)
{
  matrix.fillScreen(LOW); //Empty the screen
  matrix.setCursor(0, 0); //Move the cursor to the end of the screen
  matrix.print("WM CFG");
  matrix.write();

  DebugPrintln("Entered config mode");
  DebugPrintln(WiFi.softAPIP());
  DebugPrintln(myWiFiManager->getConfigPortalSSID());
}


void notFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webserver.uri();
  message += "\nMethod: ";
  message += (webserver.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webserver.args();
  message += "\n";

  for (uint8_t i = 0; i < webserver.args(); i++)
  {
    message += " " + webserver.argName(i) + ": " + webserver.arg(i) + "\n";
  }

  webserver.send(404, "text/plain", message);
}

void http_indexPage()
{
  String html = FPSTR(htmlHead);
  html += FPSTR(htmlStyle);
  html += FPSTR(htmlHeadEnd);
  html += FPSTR(htmlHeading);
  html += FPSTR(controls);
  html += FPSTR(htmlFooter);

  html.replace("%DEVICE_NAME%", DEVICE_NAME);

  webserver.send(200, "text/html", html);
}

void http_infoPage()
{
  // calculate uptime
  long millisecs = millis() / 1000;
  int systemUpTimeSc = millisecs % 60;
  int systemUpTimeMn = (millisecs / 60) % 60;
  int systemUpTimeHr = (millisecs / (60 * 60)) % 24;
  int systemUpTimeDy = (millisecs / (60 * 60 * 24));

  // // get SPIFFs info
  // FSInfo fs_info;
  // LittleFS.info(fs_info);

  // compose info string
  String html = FPSTR(htmlHead);
  html += FPSTR(htmlStyle);
  html += FPSTR(htmlHeadEnd);
  html += FPSTR(htmlHeading);
  html += FPSTR(info);
  html += FPSTR(htmlFooter);

  // replace placeholders
  String chipID = String(ESP.getChipId(), HEX);
  chipID.toUpperCase();

  html.replace("%DEVICE_NAME%", DEVICE_NAME);
  html.replace("%ESP.getCoreVersion%", ESP.getCoreVersion());
  html.replace("%ESP.getSdkVersion%", ESP.getSdkVersion());
  html.replace("%ESP.getResetReason%", ESP.getResetReason());
  html.replace("%loop_load_avg%", String(loop_load_avg));
  html.replace("%ESP.getFreeHeap%", String(ESP.getFreeHeap()));
  html.replace("%ESP.getHeapFragmentation%", String(ESP.getHeapFragmentation()));
  html.replace("%ESP.getChipId%", "0x" + chipID);
  html.replace("%ESP.getFlashChipId%", "0x" + String(ESP.getFlashChipId(), 16));
  html.replace("%ESP.getFlashChipRealSize%", String(ESP.getFlashChipRealSize()));
  html.replace("%ESP.getFlashChipSize%", String(ESP.getFlashChipSize()));
  html.replace("%ESP.getSketchSize%", String(ESP.getSketchSize()));
  html.replace("%ESP.getFreeSketchSpace%", String(ESP.getFreeSketchSpace()));
  // html.replace("%fs_info.usedBytes%", String(fs_info.usedBytes));
  // html.replace("%fs_info.totalBytes%", String(fs_info.totalBytes));
  html.replace("%WiFi.SSID%", WiFi.SSID());
  html.replace("%WiFi.RSSI%", String(WiFi.RSSI()));
  html.replace("%WiFi.localIP%", WiFi.localIP().toString());
  html.replace("%systemUpTimeDy%", String(systemUpTimeDy));
  html.replace("%systemUpTimeHr%", String(systemUpTimeHr));
  html.replace("%systemUpTimeMn%", String(systemUpTimeMn));
  html.replace("%systemUpTimeSc%", String(systemUpTimeSc));
  html.replace("%uptime%", String(uptime));
  webserver.send(200, "text/html", html);
}

void http_reset()
{
  webserver.send(200, "text/plain", "Restart!");
  restartDevice = true;
}

/*********************************************************************************************\
 * Sleep aware time scheduler functions borrowed from ESPEasy
\*********************************************************************************************/

inline int32_t TimeDifference(uint32_t prev, uint32_t next)
{
  return ((int32_t)(next - prev));
}

// Compute the number of milliSeconds passed since timestamp given.
// Note: value can be negative if the timestamp has not yet been reached.
int32_t TimePassedSince(uint32_t timestamp)
{
  return TimeDifference(timestamp, millis());
}

// Check if a certain timeout has been reached.
bool TimeReached(uint32_t timer)
{
  const long passed = TimePassedSince(timer);
  return (passed >= 0);
}

// Calculate next timer interval
void SetNextTimeInterval(uint32_t &timer, const uint32_t step)
{
  timer += step;
  const long passed = TimePassedSince(timer);
  if (passed < 0)
  {
    return;
  } // Event has not yet happened, which is fine.
  if (static_cast<unsigned long>(passed) > step)
  {
    // No need to keep running behind, start again.
    timer = millis() + step;
    return;
  }
  // Try to get in sync again.
  timer = millis() + (step - passed);
}

// Sleep for specified milliseconds unless data in UART buffer
void SleepDelay(uint32_t mseconds)
{
  if (mseconds)
  {
    for (uint32_t wait = 0; wait < mseconds; wait++)
    {
      delay(1);
      if (Serial.available())
      {
        break;
      } // We need to service serial buffer ASAP as otherwise we get uart buffer overrun
    }
  }
  else
  {
    delay(0);
  }
}