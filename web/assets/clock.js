function updateTimedateData() {
  fetch("/getTimedate")
    .then(function (response) {
      if (!response.ok) throw new Error("HTTP " + response.status);
      return response.json();
    })
    .then(function (data) {
      let hour = data.hour % 12 || 12; // Convert hour to 12-hour format
      let min = String(data.minute).padStart(2, "0");
      let sec = String(data.second).padStart(2, "0");
      let am_pm = data.isAM ? "AM" : "PM";
      let time = `${hour}:${min}:${sec} ${am_pm}`;

      let month = String(data.month).padStart(2, "0");
      let date = `${data.day}/${month}/${data.year}`;

      document.getElementById("time").innerHTML = time;
      document.getElementById("date").innerHTML = date;
      document.getElementById("clock-status").textContent = "";
      document.getElementById("clock-status").classList.remove("error");
    })
    .catch(function () {
      let status = document.getElementById("clock-status");
      status.textContent = "Connection lost — retrying…";
      status.classList.add("error");
    });
}

updateTimedateData();

window.setInterval(updateTimedateData, 1000); //update the clock every second
