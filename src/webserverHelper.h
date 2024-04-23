#pragma once

#include <ESP8266WebServer.h>  // Local WebServer used to serve the configuration portal
#include <globals.h>           // Global libraries and variables
#include <webserverHelper.h>  // Web server helper functions
#include <wifiHelper.h>       // WiFi helper functions

#include "webpages.h"  // Web page source code

namespace webserver
{
ESP8266WebServer webserver(80);
WiFiClient espClient;

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

  for (uint8_t i = 0; i < webserver.args(); i++) {
    message += " " + webserver.argName(i) + ": " + webserver.arg(i) + "\n";
  }

  webserver.send(404, "text/plain", message);
}

void http_indexPage()
{
  String html = FPSTR(htmlHead);
  html += FPSTR(htmlStyle);
  html += FPSTR(htmlJS);
  html += FPSTR(htmlHeadEnd);
  html += FPSTR(htmlHeading);
  html += FPSTR(htmlTime);
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
  html += FPSTR(htmlHeadRefresh);
  html += FPSTR(htmlHeadEnd);
  html += FPSTR(htmlHeading);
  html += FPSTR(info);
  html += FPSTR(htmlFooter);

  // replace placeholders
  String chipID = String(ESP.getChipId(), HEX);
  chipID.toUpperCase();

  html.replace("%REFRESH_CONTENT%", "60");
  html.replace("%DEVICE_NAME%", DEVICE_NAME);
  html.replace("%ESP.getCoreVersion%", ESP.getCoreVersion());
  html.replace("%ESP.getSdkVersion%", ESP.getSdkVersion());
  html.replace("%ESP.getResetReason%", ESP.getResetReason());
  html.replace("%loop_load_avg%", String(loop_load_avg));
  html.replace("%ESP.getFreeHeap%", String(ESP.getFreeHeap()));
  html.replace("%ESP.getHeapFragmentation%",
               String(ESP.getHeapFragmentation()));
  html.replace("%ESP.getChipId%", "0x" + chipID);
  html.replace("%ESP.getFlashChipId%", "0x" + String(ESP.getFlashChipId(), 16));
  html.replace("%ESP.getFlashChipRealSize%",
               String(ESP.getFlashChipRealSize()));
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

/**
 * @brief Handle "/config" URL request
 */
void http_configPage()
{
  // construct config page
  String html = FPSTR(htmlHead);
  html += FPSTR(htmlStyle);
  html += FPSTR(htmlHeadEnd);
  html += FPSTR(htmlHeading);
  html += FPSTR(htmlConfig);
  html += FPSTR(htmlFooter);

  html.replace("%DEVICE_NAME%", DEVICE_NAME);
  webserver.send(200, "text/html", html);
}
/**
 * @brief Handle "/configSave" URL request
 */
void http_configPageSave()
{
  String statusMsg;
  // set-time: 2024-01-01T00:00
  if (webserver.hasArg("set-time")) {
    const String dateTimeStr = webserver.arg("set-time");
    int year, month, day, hour, minute, second;

    if (sscanf(dateTimeStr.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day,
               &hour, &minute, &second) == 6) {
      setTime(hour, minute, second, day, month, year);
      statusMsg += "Time set!";
    } else {
      statusMsg += "Error setting time!";
    }
  }
  String html = FPSTR(htmlHead);
  html += FPSTR(htmlStyle);
  html += FPSTR(htmlHeadRefresh);
  html += FPSTR(htmlHeadEnd);
  html += FPSTR(htmlHeading);
  html += statusMsg + "<br />";
  html += F("Returning to main page...");
  html += FPSTR(htmlFooter);

  html.replace("%DEVICE_NAME%", DEVICE_NAME);
  html.replace("%REFRESH_CONTENT%", "3;/");

  webserver.send(200, "text/html", html);
}

/**
 * @brief Handle "/restart" URL request
 */
void http_restart()
{
  webserver.send(200, "text/plain", "Restart!");
  restartDevice = true;
}

/**
 * @brief Handle "/resetWifi" URL  request
 */
void http_resetWifi()
{
  webserver.send(200, "text/plain",
                 "Clearing WiFi credentials. You will need to reconfigure AP!");
  wifi::eraseWifi();
}

void http_sync()
{
  wifi::setupNTP(ntpUpdateInterval);
  http_indexPage();
}

void http_getTimedate()
{
  webserver.send(
      200, "application/json",
      "{\"hour\":" + String(hour()) + ", \"minute\":" + String(minute()) +
          ", \"second\":" + String(second()) + ", \"isAM\":" + String(isAM()) +
          ", \"day\":" + String(day()) + ", \"month\":" + String(month()) +
          ", \"year\":" + String(year()) + "}");
}

void setupHTTP()
{
  webserver.on("/", http_indexPage);
  webserver.on("/restart", http_restart);
  webserver.on("/info", http_infoPage);
  webserver.on("/getTimedate", http_getTimedate);
  webserver.on("/config", http_configPage);
  webserver.on("/configSave", http_configPageSave);
  webserver.on("/sync", http_sync);
  webserver.on("/resetWifi", http_resetWifi);
  webserver.onNotFound(notFound);
  webserver.begin();
}

void loopTask() { webserver.handleClient(); }
}  // namespace webserver
