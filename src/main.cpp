/*
   Some useful notes from : https://github.com/Legorobotdude/esp8266-LED-matrix-clock
   found on 21/08/2017 after having written WM, OTA and NTP code.

   Any ESP8266 module should work, just make sure you are using the right SPI pins for your module.
   You will also need a bi-directional logic level converter since the matrix runs at 5v and the
   ESP8266 at 3.3v.

   You will need wire the ESP8266's hardware SPI to the MAX7219.
   For most ESP8266 this will be as follows:

  Pins 0, 1, 2 and 4 must not be held low at boot.
  Pin 6 must not be held high at boot.

===== Oak Pin to ESP8266 GPIO Mapping =====
Oak Pin ^ P0  ^ P1  ^ P2  ^ P3  ^ P4  ^ P5  ^ P6  ^ P7  ^ P8  ^ P9  ^ P10 ^ P11 ^
GPIO    | 2   | 5   | 0   | 3   | 1   | 4   | 15  | 13  | 12  | 14  | 16  | 17  |


          Enable    |      VIN        X - Provide 5v to rest of circuit
          Reset     |      GND        X
    P11 / A0  / 17  |  4 / P5         A?
D  Wake / P10 / 16  |  1 / P4 / TX    TX - don't hold low
D  SCLK / P9  / 14  |  3 / P3 / RX    RX
   MISO / P8  / 12  |  0 / P2 / SCL   B  - don't hold low
D  MOSI / P7  / 13  |  5 / P1 / LED   LED
     SS / P6  / 15  |  2 / P0 / SDA   A? - don't hold low
          GND       |      VCC        X - 3v3 - level shifter + AXDL

D - Display
B - Button
A - Accelerometer

   SPI CLK  = GPIO14
   SPI MOSI = GPIO13
   SPI MISO = GPIO12

   Wire CLK to CLK and MOSI to DIN.

   The CS pin from the 7219 can be connected to any GPIO, just define it in the code.
   The default is 16.
*/

#define HOSTNAME "ntp-clock-2"

#include <Arduino.h>
#include <ESP8266WiFi.h> //ESP8266 Core WiFi Library (you most likely already have this in your sketch)

#include <DNSServer.h>        //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h> //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>      //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <ESP8266mDNS.h>
#include <WiFiUdp.h> // For NTP

#include <ArduinoOTA.h> // For OTA programming
#include <TimeLib.h>    // Friendly Time formatting and timekeeping

#include <SPI.h> // For LED Panel
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>

#include <Wire.h>
#include <MPU6050_tockn.h>
MPU6050 mpu6050(Wire);

#include <Button.h>
Button button(0); // Connect your button between P2/GPIO0 and GND

#include <elapsedMillis.h>
elapsedMillis orientationCheck;
const unsigned int orientationCheckInterval = 500;

#define DEBUG
#define DEBUG_OI Serial

#ifdef DEBUG
#define DebugBegin(...) DEBUG_OI.begin(__VA_ARGS__)
#define DebugPrint(...) DEBUG_OI.print(__VA_ARGS__)
#define DebugPrintln(...) DEBUG_OI.println(__VA_ARGS__)
#define DebugPrintf(...) DEBUG_OI.printf(__VA_ARGS__)
#define DebugWrite(...) DEBUG_OI.write(__VA_ARGS__)
#else
// Define the counterparts that cause the compiler to generate no code
#define DebugBegin(...) (void(0))
#define DebugPrint(...) (void(0))
#define DebugPrintln(...) (void(0))
#define DebugPrintf(...) (void(0))
#define DebugWrite(...) (void(0))
#endif

/************************* NTP SETUP ****************************************/
static const char ntpServerName[] = "au.pool.ntp.org";
const int timeZone = 10; // AEST

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
unsigned int localPort = 2390; // local port to listen for UDP packets

/************************* LED MATRIX SETUP *********************************/
int pinCS = 16;                     // Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
int numberOfHorizontalDisplays = 4; //adjust this to your setup
int numberOfVerticalDisplays = 1;

Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

bool useIMU = true;
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
double pitch, roll, yaw;

/************************* FUNCTION PROTOTYPES ******************************/
time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);
void setDisplayOrientation();
void setupOTA();
void setupWifi();
void configModeCallback(WiFiManager *myWiFiManager);

void setup()
{
  DebugBegin(115200);

  DebugPrintln("");
  DebugPrintln(F("ESP8266 NTP Clock loading..."));
  DebugPrint("Compiled: " __DATE__ ", " __TIME__ ", Arduino IDE v");
  DebugPrintln(ARDUINO, DEC);
  DebugPrint(F("Core version: "));
  DebugPrintln(ESP.getCoreVersion());
  DebugPrint(F("SDK version: "));
  DebugPrintln(ESP.getSdkVersion());
  DebugPrint(F("Chip ID: "));
  DebugPrintln(ESP.getChipId());
  DebugPrint(F("Reset reason: "));
  DebugPrintln(ESP.getResetReason());
  DebugPrint(F("Heap free: "));
  DebugPrintln(ESP.getFreeHeap());
  DebugPrintln("");

  // Initalise GY-521 / MPU-6050
  Wire.begin(P0, P5); //SDA, SCL
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
  ArduinoOTA.handle();
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
    String tape = "Let go of me!";
    int wait = 30; // In milliseconds - was 20

    unsigned int spacer = 1;
    unsigned int width = 5 + spacer; // The font width is 5 pixels

    for (unsigned int i = 0; i < width * tape.length() + matrix.width() - 1 - spacer; i++)
    {

      matrix.fillScreen(LOW);

      unsigned int letter = i / width;
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

  delay(10);
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
    //    //redundant, screen is blank
    //    matrix.drawPixel(12, 2, LOW);
    //    matrix.drawPixel(12, 4, LOW);
    //  }
    //  else
    //  {
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
    //matrix.print("A");

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
    // matrix.print("P");
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

  // digital clock display of the time
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
    DebugPrint(Y);

    if (Y <= -40)
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
    else if (Y >= 40)
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
  //wifiManager.setConfigPortalTimeout(180);

#ifndef DEBUG
  wifiManager.setDebugOutput(false);
#endif

  matrix.fillScreen(LOW); //Empty the screen
  matrix.setCursor(0, 0); //Move the cursor to the end of the screen
  matrix.print("WiFi");
  matrix.write();

  if (!wifiManager.autoConnect())
  {
    DebugPrintln("Failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
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
