#pragma once

#include <ArduinoOTA.h>     // OTA programming support
#include <DNSServer.h>      // DNS Server for redirect to configuration portal
#include <ESP8266WiFi.h>    // ESP8266 Core WiFi Library
#include <ESP8266mDNS.h>    // Multicast DNS (for OTA)
#include <WiFiManager.h>    // WiFi Configuration Portal
#include <WiFiUdp.h>        // UDP support (for NTP)
#include <displayHelper.h>  // Display helper functions
#include <globals.h>        // Global libraries and variables

namespace wifi
{
// NTP Setup
constexpr const char ntpServerName[] = "au.pool.ntp.org";

WiFiUDP udp;  // A UDP instance to let us send and receive packets over UDP
constexpr uint16_t localPort = 2390;  // local port to listen for UDP packets

constexpr uint8_t NTP_PACKET_SIZE =
    48;  // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];  // buffer to hold incoming & outgoing
                                     // packets

uint32_t last_event = 0;   // Last wiFi connection event
uint32_t downtime = 0;     // WiFi down duration
const int haltDelay = 200; // delay in ms before webserver/wifi halted

WiFiManager wifiManager;

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  DebugPrintln("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123);  // NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

time_t getNtpTime()
{
  IPAddress ntpServerIP;  // NTP server's ip address

  while (udp.parsePacket() > 0)
    ;  // discard any previously received packets
  DebugPrintln("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  DebugPrint(ntpServerName);
  DebugPrint(": ");
  DebugPrintln(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      DebugPrintln("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
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
  return 0;  // return 0 if unable to get the time
}

void setupOTA()
{
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  ArduinoOTA.onStart([]() {
    DebugPrintln("OTA Programming Start");
    display::printMsg("OTA");
  });

  ArduinoOTA.onEnd([]() {
    DebugPrintln("\nOTA Programming End");
    display::printMsg("DONE!");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DebugPrintf("Progress: %u%%\r", (progress / (total / 100)));
    display::printProgress(progress, total);
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

    display::printMsg("OTA ER");
  });

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.begin();
  DebugPrintln("*OTA: Ready");
}

void WifiSetState(uint8_t state)
{
    if (state) {
      downtime += uptime - last_event;
    } else {
      last_event = uptime;
    }
}

void WifiCheckState(void) {
  if ((WL_CONNECTED == WiFi.status()) && (uint32_t)WiFi.localIP() != 0) {
    WifiSetState(1);
  } else {
    WifiSetState(0);
  }
}

void otaLoopTask()
{
  // Allow MDNS discovery
  MDNS.update();

  // Poll / handle OTA programming
  ArduinoOTA.handle();
}

void configModeCallback(WiFiManager *myWiFiManager)
{
  display::printMsg("WM CFG");

  DebugPrintln("Entered config mode");
  DebugPrintln(WiFi.softAPIP());
  DebugPrintln(myWiFiManager->getConfigPortalSSID());
}

void setupNTP(unsigned int syncInterval)
{
  setSyncProvider(getNtpTime);
  setSyncInterval(syncInterval);  // every eight hours
}

void setupUDP()
{
  udp.begin(localPort);
  DebugPrint("*UDP: Running on local port ");
  DebugPrintln(udp.localPort());
}

void setupWifi()
{
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setConfigPortalTimeout(300);  // 5 minute timout

#ifndef DEBUG
  wifiManager.setDebugOutput(false);
#endif

  display::printMsg("WiFi");

  if (!wifiManager.autoConnect(OTA_HOSTNAME)) {
    DebugPrintln("Failed to connect and hit timeout");
    // reset and try again
    ESP.reset();
    delay(1000);
  }

  WiFi.hostname(OTA_HOSTNAME);
}

void eraseWifi()
{
  display::printMsg("CLR WiFi");
  delay(haltDelay);
  wifiManager.resetSettings();
  ESP.eraseConfig();
  WiFi.disconnect();
  delay(haltDelay);
  ESP.restart();
  delay(haltDelay);
}
}  // namespace wifi