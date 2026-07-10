# Quickstart

This guide gets **Your Pet Desk: DigiBat** running on an ESP32 with a small
OLED display.

For day-to-day usage, also read `docs/firmware-user-guide.md`.

## 1. Minimum Hardware

- ESP32 NodeMCU-32S / ESP-32S Kit.
- 128x64 I2C OLED, ideally a 1.3" SH1106 module.
- 3 navigation buttons.
- Optional red LED.
- USB cable for the first flash.

DigiBat works without Proxmox. Proxmox, the weather station, and OTA improve
the dashboard side of the project, but they are not required to play with the
virtual pet.

## 2. Minimum Wiring

OLED:

| OLED | ESP32 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 22 |
| SCL/SCK | GPIO 23 |

Buttons:

| Button | ESP32 |
| --- | --- |
| Next | GPIO 25 |
| Select / OK | GPIO 33 |
| Previous | GPIO 32 |

Wire each button between its GPIO and `GND`. The firmware uses the ESP32
internal pull-ups.

Optional red LED:

| Red LED | ESP32 |
| --- | --- |
| Anode through resistor | GPIO 27 |
| Cathode | GND |

A `220 ohms` or `330 ohms` resistor is fine.

## 3. Configuration

Copy the example configuration:

```sh
cp include/config.example.h include/config.h
```

At minimum, fill in:

```cpp
#define WIFI_SSID "YOUR_WIFI"
#define WIFI_PASSWORD "YOUR_PASSWORD"
```

If you only want to test the pet, you can leave Proxmox and weather with
placeholder URLs. The DigiBat home page remains usable even when those services
do not answer.

For the full dashboard, also configure:

```cpp
#define PROXMOX_METRICS_URL "http://BRIDGE_IP:8080/metrics"
#define WEATHER_URL "http://WEATHER_STATION_IP/api"
```

`PROXMOX_METRICS_URL` must point to the project's HTTP bridge, not to the
Proxmox web interface.

## 4. First USB Flash

Plug the ESP32 over USB, then run:

```sh
pio run -e nodemcu-32s --target upload
```

You can then open the serial monitor:

```sh
pio device monitor
```

The first useful screen is the DigiBat home page.

## 5. OLED Diagnostic

If the screen stays black, test the OLED only:

```sh
pio run -e oled-test --target upload
pio device monitor
```

The screen should show:

```text
OLED TEST
SDA22 SCK23
SH1106 SW I2C
```

If nothing appears, check `VCC`, `GND`, `SDA`, and `SCL/SCK` first.

## 6. Basic Use

- `Next` and `Previous` change pages.
- On the home page, `Select` opens the pet menu.
- `STATS` shows DigiBat's current information.
- `HUNT` earns food.
- `EAT` restores hunger.
- `SLEEP` restores energy.
- `ARENA` earns XP and levels.
- `SCREEN OFF` turns the OLED off.

When the screen is off, any button wakes it.

## 7. OTA Later

After the first USB flash, updates can use Wi-Fi OTA if the ESP32 is connected
and the OTA password is configured.

The `NETWORK` page shows OTA state:

- `ON :3232`: OTA is ready;
- `OFF WIFI`: Wi-Fi is unavailable;
- `OFF INIT`: OTA has not initialized yet.

Only try the first OTA flash after confirming that the USB-flashed firmware
boots correctly.
