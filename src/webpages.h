#pragma once

#include <globals.h>  // Global libraries and variables

constexpr char htmlHead[] PROGMEM = R"=====(
<!DOCTYPE html><html lang="en">
<meta name="viewport"content="width=device-width,initial-scale=1"/><head>
<title>%DEVICE_NAME%</title>
)=====";

constexpr char htmlStyle[] PROGMEM = R"=====(
<style>
.c{text-align:center;}div,input{padding:5px;font-size:1em;}
input{width:95%;}body{text-align:center;font-family:verdana;}
button{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} 
.q{float:right;width:64px;text-align:right;} 
.l{background:url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAALVBMVEX///8EBwfBwsLw8PAzNjaCg4NTVVUjJiZDRUUUFxdiZGSho6OSk5Pg4eFydHTCjaf3AAAAZElEQVQ4je2NSw7AIAhEBamKn97/uMXEGBvozkWb9C2Zx4xzWykBhFAeYp9gkLyZE0zIMno9n4g19hmdY39scwqVkOXaxph0ZCXQcqxSpgQpONa59wkRDOL93eAXvimwlbPbwwVAegLS1HGfZAAAAABJRU5ErkJggg==")no-repeat left center;background-size:1em;}
</style>
)=====";

constexpr char htmlHeadEnd[] PROGMEM = R"=====(
</head><body><div style="text-align:left;display:inline-block;min-width:260px;">
)=====";

constexpr char htmlHeading[] PROGMEM = R"=====(<h1>%DEVICE_NAME%</h1>)=====";
constexpr char htmlFooter[] PROGMEM = R"=====(</div></body></html>)=====";

constexpr char htmlJS[] PROGMEM = R"=====(
    <script>
        function updateTimedateData() {
            console.log("updateTimedateData()");
            let timedate = new XMLHttpRequest();

            timedate.onreadystatechange = function () {
                if (this.readyState == 4 && this.status == 200) {
                    console.log("updateTimedateData(): got Data:", this.responseText);
                    let data = JSON.parse(this.responseText);

                    let min = String(data.minute).padStart(2, '0');
                    let sec = String(data.second).padStart(2, '0');
                    let am_pm = data.isAM ? "AM" : "PM";
                    let time = `${data.hour}:${min}:${sec} ${am_pm}`;

                    let month = String(data.month).padStart(2, '0');
                    let date = `${data.day}/${month}/${data.year}`;

                    document.getElementById("time").innerHTML = time;
                    document.getElementById("date").innerHTML = date;
                }
            };
            timedate.open("GET", "/getTimedate", true);
            timedate.send();
        }

        updateTimedateData();

        window.setInterval(updateTimedateData, 1000); //update the clock every second
    </script>
)=====";

constexpr char htmlTime[] PROGMEM = R"=====(
<div id="time"></div>
<div id="date"></div>
)=====";

constexpr char controls[] PROGMEM = R"=====(
<form action="/info" method="get"><button>Info</button></form><br/>
)=====";

constexpr char info[] PROGMEM = R"=====(
<b>ESP8266 Core Version:</b> %ESP.getCoreVersion%<br />
<b>ESP8266 SDK Version:</b> %ESP.getSdkVersion%<br />
<br />
<b>Reset Reason:</b> %ESP.getResetReason%<br />
<br />
<b>Load Average:</b> %loop_load_avg%<br />
<b>Free Heap:</b> %ESP.getFreeHeap% bytes (%ESP.getHeapFragmentation%% fragmentation)<br />
<br />
<b>ESP8266 Chip ID:</b> %ESP.getChipId%<br />
<b>ESP8266 Flash Chip ID:</b> %ESP.getFlashChipId%<br />
<br />
<b>Flash Chip Size:</b> %ESP.getFlashChipRealSize% bytes (%ESP.getFlashChipSize% bytes seen by SDK)<br />
<b>Sketch Size:</b> %ESP.getSketchSize% bytes used of %ESP.getFreeSketchSpace% bytes available<br />
<br />
<b>WiFi SSID:</b> %WiFi.SSID%<br />
<b>WiFi RSSI:</b> %WiFi.RSSI%dBm<br />
<b>WiFi IP:</b> %WiFi.localIP%<br />
<br />
<b>System Uptime:</b> %systemUpTimeDy% day(s), %systemUpTimeHr% hour(s), %systemUpTimeMn% minute(s), %systemUpTimeSc% second(s)<br />
<b>Uptime (seconds):</b> %uptime%<br />
<br /><br />
<a href="/restart"><button>Restart</button></a>
<br /><br />
<a href="/"><button>Back</button></a>
)=====";
