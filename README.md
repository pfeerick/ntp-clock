# NTP Clock

## GPIO usage/pinout
 
### Wemos D1 Mini:
```

      /----------------------\
      |     |?|?|_|?|_|?|    |
      |     | |         |    |
  [ ] | RST          LED| TX | [ ]
  [ ] | A0  |???????????| RX | [ ]
  [M] | D0  |           | D1 | [G]
  [M] | D5  |           | D2 | [G]
  [ ] | D6  |  ESP8266  | D3 | [B]
  [M] | D7  |           | D4 | [ ]
  [ ] | D8  |___________|  G | [ ]
  [ ] | 3V3               5V | [ ]
       \                     |
       [| RST        D1 Mini |
        |_____|  USB  |______|
 
 [M] MAX72xx Panel
     IO16 - D0 - CS
     IO14 - D5 - CLK
     IO13 - D7 - MOSI
 
 [G] GY-521 / MPU-6050
     IO5  - D1 - SCL
     IO4  - D2 - SDA
 
 [B] Button
     IO0  - D3  - Button -> GND
```
 
### Digistump Oak:
```
 
           Enable    |      VIN        X - Provide 5v to rest of circuit
           Reset     |      GND        X
     P11 / A0  / 17  |  4 / P5         A
 D  Wake / P10 / 16  |  1 / P4 / TX    TX - don't hold low at boot
 D  SCLK / P9  / 14  |  3 / P3 / RX    RX
    MISO / P8  / 12  |  0 / P2 / SCL   B  - don't hold low at boot
 D  MOSI / P7  / 13  |  5 / P1 / LED   LED
      SS / P6  / 15  |  2 / P0 / SDA   A - don't hold low at boot
           GND       |      VCC        X - 3v3 - level shifter + AXDL
 
 D - Display
 B - Button
 A - Accelerometer
 ```