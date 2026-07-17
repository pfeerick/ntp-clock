function updateTimedateData() {
    console.log("updateTimedateData()");
    let timedate = new XMLHttpRequest();

    timedate.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            console.log("updateTimedateData(): got Data:", this.responseText);
            let data = JSON.parse(this.responseText);

            let hour = data.hour % 12 || 12; // Convert hour to 12-hour format
            let min = String(data.minute).padStart(2, '0');
            let sec = String(data.second).padStart(2, '0');
            let am_pm = data.isAM ? "AM" : "PM";
            let time = `${hour}:${min}:${sec} ${am_pm}`;

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
