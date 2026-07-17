#pragma once

#include <ESP8266WebServer.h>  // Local WebServer used to serve the configuration portal
#include <globals.h>           // Global libraries and variables
#include <wifiHelper.h>       // WiFi helper functions

#include "generated/webpages.h"  // Web page source code (generated from web/ by scripts/generate_webpages.py)

namespace webserver
{
ESP8266WebServer webserver(80);

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
  String html = FPSTR(page_index);

  html.replace("%DEVICE_NAME%", DEVICE_NAME);

  webserver.send(200, "text/html", html);
}

void http_infoPage()
{
  // calculate uptime
  uint32_t upSeconds = uptime;
  int systemUpTimeSc = upSeconds % 60;
  int systemUpTimeMn = (upSeconds / 60) % 60;
  int systemUpTimeHr = (upSeconds / (60 * 60)) % 24;
  int systemUpTimeDy = (upSeconds / (60 * 60 * 24));

  // // get SPIFFs info
  // FSInfo fs_info;
  // LittleFS.info(fs_info);

  // compose info string
  String html = FPSTR(page_info);

  // replace placeholders
  String chipID = String(ESP.getChipId(), HEX);
  chipID.toUpperCase();

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
  String html = FPSTR(page_config);

  char currentDateTime[20];
  snprintf(currentDateTime, sizeof(currentDateTime), "%04d-%02d-%02dT%02d:%02d:%02d",
           year(), month(), day(), hour(), minute(), second());

  html.replace("%DEVICE_NAME%", DEVICE_NAME);
  html.replace("%CURRENT_DATETIME%", currentDateTime);
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
    int year, month, day, hour, minute;
    int second = 0;

    if (sscanf(dateTimeStr.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day,
               &hour, &minute, &second) >= 5) {
      setTime(hour, minute, second, day, month, year);
      statusMsg += "Time set!";
    } else {
      statusMsg += "Error setting time!";
    }
  }
  String html = FPSTR(page_config_save);

  html.replace("%DEVICE_NAME%", DEVICE_NAME);
  html.replace("%STATUS_MSG%", statusMsg);

  webserver.send(200, "text/html", html);
}

/**
 * @brief Handle "/restart" URL request
 */
void http_restart()
{
  String html = FPSTR(page_restart);
  html.replace("%DEVICE_NAME%", DEVICE_NAME);
  webserver.send(200, "text/html", html);
  restartDevice = true;
}

/**
 * @brief Handle "/resetWifi" URL  request
 */
void http_resetWifi()
{
  String html = FPSTR(page_reset_wifi);
  html.replace("%DEVICE_NAME%", DEVICE_NAME);
  html.replace("%HOSTNAME%", HOSTNAME);
  webserver.send(200, "text/html", html);
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

/**
 * @brief Handle "/getInfo" URL request -- JSON payload of the info page's
 * fields that actually change over time, polled by web/assets/info.js so
 * "/info" no longer needs a full-page <meta http-equiv="refresh">.
 */
void http_getInfo()
{
  uint32_t upSeconds = uptime;
  int systemUpTimeSc = upSeconds % 60;
  int systemUpTimeMn = (upSeconds / 60) % 60;
  int systemUpTimeHr = (upSeconds / (60 * 60)) % 24;
  int systemUpTimeDy = (upSeconds / (60 * 60 * 24));

  webserver.send(
      200, "application/json",
      "{\"loadAvg\":" + String(loop_load_avg) +
          ", \"freeHeap\":" + String(ESP.getFreeHeap()) +
          ", \"heapFragmentation\":" + String(ESP.getHeapFragmentation()) +
          ", \"rssi\":" + String(WiFi.RSSI()) +
          ", \"uptimeDy\":" + String(systemUpTimeDy) +
          ", \"uptimeHr\":" + String(systemUpTimeHr) +
          ", \"uptimeMn\":" + String(systemUpTimeMn) +
          ", \"uptimeSc\":" + String(systemUpTimeSc) +
          ", \"uptime\":" + String(uptime) + "}");
}

/**
 * @brief Handlers for gzip'd static assets (CSS/JS under web/assets/),
 * embedded as PROGMEM byte arrays by scripts/generate_webpages.py. These have no
 * runtime %PLACEHOLDER% substitution, so they're served verbatim -- no
 * String copy -- with Content-Encoding/Cache-Control headers.
 */
void http_styleCss()
{
  webserver.sendHeader(F("Content-Encoding"), F("gzip"));
  webserver.sendHeader(F("Cache-Control"), F("public, max-age=86400"));
  webserver.send_P(200, "text/css", (PGM_P)asset_style_css_gz, asset_style_css_gz_len);
}

void http_clockJs()
{
  webserver.sendHeader(F("Content-Encoding"), F("gzip"));
  webserver.sendHeader(F("Cache-Control"), F("public, max-age=86400"));
  webserver.send_P(200, "application/javascript", (PGM_P)asset_clock_js_gz, asset_clock_js_gz_len);
}

void http_infoJs()
{
  webserver.sendHeader(F("Content-Encoding"), F("gzip"));
  webserver.sendHeader(F("Cache-Control"), F("public, max-age=86400"));
  webserver.send_P(200, "application/javascript", (PGM_P)asset_info_js_gz, asset_info_js_gz_len);
}

void setupHTTP()
{
  webserver.on("/", http_indexPage);
  webserver.on("/restart", http_restart);
  webserver.on("/info", http_infoPage);
  webserver.on("/getTimedate", http_getTimedate);
  webserver.on("/getInfo", http_getInfo);
  webserver.on("/config", http_configPage);
  webserver.on("/configSave", http_configPageSave);
  webserver.on("/sync", http_sync);
  webserver.on("/resetWifi", http_resetWifi);
  webserver.on("/style.css", http_styleCss);
  webserver.on("/clock.js", http_clockJs);
  webserver.on("/info.js", http_infoJs);
  webserver.onNotFound(notFound);
  webserver.begin();
}

void loopTask() { webserver.handleClient(); }
}  // namespace webserver
