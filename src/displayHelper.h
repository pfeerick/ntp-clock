#pragma once

#include <globals.h>             // Global libraries and variables
#include <Adafruit_GFX.h>        // Adafruit GFX routines
#include <Adafruit_I2CDevice.h>  // Adafruit I2C device support
#include <Max72xxPanel.h>        // MAX72xx multiplexer
#include <SPI.h>                 // SPI device support
#include <sensorHelper.h>        // Sensor helper functions

namespace display
{
constexpr uint8_t pinCS = 16;  // Attach CS to this pin, DIN to MOSI and CLK to
                               // SCK (cf http://arduino.cc/en/Reference/SPI )
constexpr uint8_t numHorzDisp = 4;  // Adjust this to your setup
constexpr uint8_t numVertDisp = 1;

constexpr uint8_t displayUp = 0;
constexpr uint8_t displayDown = 1;

Max72xxPanel matrix = Max72xxPanel(pinCS, numHorzDisp, numVertDisp);

void setRotation(int y)
{
  if (y == 0) {
    // right way up
    matrix.setRotation(0, 1);
    matrix.setRotation(1, 1);
    matrix.setRotation(2, 1);
    matrix.setRotation(3, 1);

    // left to right
    matrix.setPosition(0, 0, 0);  // The first display is at <0, 0>
    matrix.setPosition(1, 1, 0);  // The second display is at <1, 0>
    matrix.setPosition(2, 2, 0);  // The third display is at <2, 0>
    matrix.setPosition(3, 3, 0);  // And the last display is at <3, 0>
  } else if (y == 1) {
    // upside down
    matrix.setRotation(0, 3);
    matrix.setRotation(1, 3);
    matrix.setRotation(2, 3);
    matrix.setRotation(3, 3);

    // now right to left
    matrix.setPosition(0, 3, 0);  // The first display is at <3, 0>
    matrix.setPosition(1, 2, 0);  // The second display is at <2, 0>
    matrix.setPosition(2, 1, 0);  // The third display is at <1, 0>
    matrix.setPosition(3, 0, 0);  // And the last display is at <0, 0>
  }
}

void printMsg(String msg)
{
  matrix.fillScreen(LOW);  // Empty the screen
  matrix.setTextSize(1);
  matrix.setCursor(0, 0);  // Move the cursor to the end of the screen
  matrix.print(msg);
  matrix.write();
}

void printProgress(int progress, int total)
{
  matrix.fillScreen(LOW);  // Empty the screen
  matrix.setTextSize(1);
  matrix.setCursor(0, 0);  // Move the cursor to the end of the screen
  matrix.print("P");

  // narrow space colon
  matrix.drawPixel(7, 2, HIGH);
  matrix.drawPixel(7, 4, HIGH);

  matrix.setCursor(9, 0);
  if ((progress / (total / 100)) < 10) {
    matrix.print("0");
  }
  matrix.print((progress / (total / 100)));
  matrix.print("%");
  matrix.write();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  DebugPrint(":");
  if (digits < 10) DebugPrint('0');
  DebugPrint(digits);
}

void scrollingText(String msg, uint8_t animationSpeed)
{
  constexpr int spacer = 1;
  constexpr int width = 5 + spacer;  // The font width is 5 pixels

  for (unsigned int i = 0;
       i < width * msg.length() + matrix.width() - 1 - spacer; i++) {
    matrix.fillScreen(LOW);

    unsigned int letter = i / width;
    int x = (matrix.width() - 1) - i % width;
    int y = (matrix.height() - 8) / 2;  // center the text vertically

    while (x + width - spacer >= 0 && letter >= 0) {
      if (letter < msg.length()) {
        matrix.drawChar(x, y, msg[letter], HIGH, LOW, 1);
      }

      letter--;
      x -= width;
    }

    matrix.write();  // Send bitmap to display
    delay(animationSpeed);
  }
}

void digitalClockDisplay()
{
  matrix.fillScreen(LOW);  // Empty the screen
  matrix.setCursor(0, 0);  // Move the cursor to the end of the screen
  if (hourFormat12() < 10) matrix.print(" ");

  matrix.print(hourFormat12());  // Write the time

  if ((second() % 2) != 0) {
    matrix.drawPixel(12, 2, HIGH);
    matrix.drawPixel(12, 4, HIGH);
  }

  matrix.setCursor(14, 0);  // Move the cursor to the end of the screen
  if (minute() < 10) {
    matrix.print("0");
  }
  matrix.print(minute());

  matrix.setCursor(27, 0);
  if (isAM()) {
    // draw an A
    matrix.drawPixel(28, 2, HIGH);  // *
    matrix.drawPixel(28, 3, HIGH);  // *
    matrix.drawPixel(28, 4, HIGH);  // *
    matrix.drawPixel(28, 5, HIGH);  // *
    matrix.drawPixel(28, 6, HIGH);  // *

    matrix.drawPixel(29, 1, HIGH);  //**
    matrix.drawPixel(29, 4, HIGH);  //**

    matrix.drawPixel(30, 1, HIGH);  //***
    matrix.drawPixel(30, 4, HIGH);  //***

    matrix.drawPixel(31, 2, HIGH);  // *
    matrix.drawPixel(31, 3, HIGH);  // *
    matrix.drawPixel(31, 4, HIGH);  // *
    matrix.drawPixel(31, 5, HIGH);  // *
    matrix.drawPixel(31, 6, HIGH);  // *
  } else {
    // draw a P
    matrix.drawPixel(28, 1, HIGH);  // *
    matrix.drawPixel(28, 2, HIGH);  // *
    matrix.drawPixel(28, 3, HIGH);  // *
    matrix.drawPixel(28, 4, HIGH);  // *
    matrix.drawPixel(28, 5, HIGH);  // *
    matrix.drawPixel(28, 6, HIGH);  // *

    matrix.drawPixel(29, 1, HIGH);  //**
    matrix.drawPixel(29, 4, HIGH);  //**

    matrix.drawPixel(30, 1, HIGH);  //***
    matrix.drawPixel(30, 4, HIGH);  //***

    matrix.drawPixel(31, 2, HIGH);  //   *
    matrix.drawPixel(31, 3, HIGH);  //   *
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

void setDisplayOrientation(float Y)
{
  if (Y >= 40) {
    DebugPrintln("Display up");
    display::setRotation(displayUp);
  } else if (Y <= -40) {
    DebugPrintln("Display down");
    display::setRotation(displayDown);
  } else {
    DebugPrintln("Display don't know");
    display::setRotation(displayUp);
  }
}

void setup(uint8_t brightness)
{
  matrix.setIntensity(brightness);  // Set brightness between 0 and 15
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.setTextColor(HIGH);
}

}  // namespace display
