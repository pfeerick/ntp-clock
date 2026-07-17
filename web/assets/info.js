function updateInfoData() {
  fetch("/getInfo")
    .then(function (response) {
      if (!response.ok) throw new Error("HTTP " + response.status);
      return response.json();
    })
    .then(function (data) {
      document.getElementById("load-avg").textContent = data.loadAvg;
      document.getElementById("free-heap").textContent =
        `${data.freeHeap} bytes (${data.heapFragmentation}% fragmentation)`;
      document.getElementById("wifi-rssi").textContent = `${data.rssi}dBm`;
      document.getElementById("uptime-readable").textContent =
        `${data.uptimeDy} day(s), ${data.uptimeHr} hour(s), ${data.uptimeMn} minute(s), ${data.uptimeSc} second(s)`;
      document.getElementById("uptime-seconds").textContent = data.uptime;
    })
    .catch(function () {
      // Silently keep showing the last known values; the fields simply
      // stop refreshing until connectivity to the device is restored.
    });
}

updateInfoData();

window.setInterval(updateInfoData, 5000);
