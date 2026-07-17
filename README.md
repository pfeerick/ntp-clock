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

## Web UI development

The web UI (index/info/config pages) lives as real HTML/CSS/JS sources under
`web/` (`web/pages/`, `web/partials/`, `web/assets/`), not as PROGMEM strings
in the firmware source. At build time, `scripts/generate_webpages.py` (a
PlatformIO `pre:` extra_script, stdlib Python only) composes those sources
into `src/generated/webpages.h` -- a gitignored, auto-generated header with
one PROGMEM constant per page, plus a gzip-compressed PROGMEM byte array for
each static asset under `web/assets/` (`style.css`, `clock.js`, `info.js`),
served from their own `Content-Encoding: gzip` routes rather than inlined
into every page. This runs automatically as part of every `pio run`, so
**no extra tooling is required to build the firmware**.

Because `src/generated/webpages.h` doesn't exist until the first
`pio run`, your editor may show a missing-include squiggle on
`#include "generated/webpages.h"` in `src/webserverHelper.h` until you've
built the project at least once.

To iterate on the pages in a browser instead of flashing hardware, this repo
includes a small zero-dependency [bun](https://bun.sh) dev server that
re-composes `web/` on every request and serves it with mocked runtime values
(`web/mock-values.json`):

```sh
bun run dev
```

Then browse to <http://localhost:8266>. `bun --watch` restarts the server
whenever a file under `web/` changes. Bun is dev-only -- it is never invoked
by the firmware build or CI.

The dev server keeps a virtual RTC, so it behaves like real hardware: setting
the time via Configure persists and free-runs from there, while Sync, the
automatic NTP re-sync (`ntpSyncIntervalSeconds` in `mock-values.json`), and
`/restart` all snap it back to true time. This state resets whenever
`--watch` restarts the server on a file edit.