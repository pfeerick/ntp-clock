#pragma once

#include <globals.h>      // Global libraries and variables
#include <ESP8266WiFi.h>  // ESP8266 Core WiFi Library

namespace sleep
{

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
  if (passed < 0) {
    return;
  }  // Event has not yet happened, which is fine.
  if (static_cast<unsigned long>(passed) > step) {
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
  if (mseconds) {
    for (uint32_t wait = 0; wait < mseconds; wait++) {
      delay(1);
      if (Serial.available()) {
        break;
      }  // We need to service serial buffer ASAP as otherwise we get uart
         // buffer overrun
    }
  } else {
    delay(0);
  }
}

void dynamicSleep(uint32_t my_sleep)
{
  // Dynamic sleep
  uint32_t my_activity =
      millis() - my_sleep;  // Time this loop has taken in milliseconds
  if (my_activity < sleepTime) {
    SleepDelay(sleepTime -
               my_activity);  // Provide time for background tasks like wifi
  } else {
    if (WiFi.status() != WL_CONNECTED) {
      SleepDelay(my_activity / 2);  // If wifi down then force loop delay to 1/2
                                    // of my_activity period
    }
  }

  // Calculate loop load_avg
  if (!my_activity) {
    my_activity++;
  }  // We cannot divide by 0
  uint32_t loop_delay = sleepTime;
  if (!loop_delay) {
    loop_delay++;
  }  // We cannot divide by 0
  uint32_t loops_per_second =
      1000 / loop_delay;  // We need to keep track of this many loops per second
  uint32_t this_cycle_ratio = 100 * my_activity / loop_delay;
  loop_load_avg =
      loop_load_avg - (loop_load_avg / loops_per_second) +
      (this_cycle_ratio / loops_per_second);  // Take away one loop average away
                                              // and add the new one
}

void updateUptime()
{
  static uint32_t state_second = 0;  // State second timer
  if (TimeReached(state_second)) {
    SetNextTimeInterval(state_second, 1000);
    uptime++;
  }
}
}  // namespace sleep