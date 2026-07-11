# Your Desk Companion: DigiBat

A tiny ESP32 OLED virtual pet desk companion.

**Your Desk Companion: DigiBat** is an ESP32 firmware for a small OLED screen. It
turns a desk module into a pixel-art companion: a digital bat you can feed, let
sleep, send hunting, and train in arena battles between coding sessions,
ChatGPT chats, or Claude Code runs.

The project, also called **YDC**, mixes a desk dashboard with a virtual pet
inspired by creature-training games, handheld digital pets, and always-on desk
companions.

Main features:

- `DigiBat` desk companion with hunger, energy, food, level, XP, and stats;
- virtual pet actions: `EAT`, `SLEEP`, `HUNT`, `ARENA`, `STATS`, and `RESET PET`;
- 128x64 OLED display with pixel-art sprites, animations, and compact gauges;
- optional dashboard pages for Proxmox, network, VMs, LXC containers, and local weather;
- onboard settings to enable or disable Proxmox and the weather station;
- Wi-Fi OTA updates after the first USB flash.

For the fastest setup, start with `QUICKSTART.md`. For daily use, read
`docs/firmware-user-guide.md`.

## Licenses and Attribution

The bat sprites used by DigiBat are adapted from `Pixel Art Bat 1BIT` by
`dustdfg` and are distributed under `CC BY-SA 4.0`. See
`assets/tamagotchi/ATTRIBUTION.md` and `licenses/CC-BY-SA-4.0.txt`.

## Architecture Recommendation

Do not SSH from the ESP32 into Proxmox. It is fragile, heavy, and would force
you to embed sensitive secrets in the microcontroller.

The best V1 architecture is:

1. Proxmox exposes status through its local API with a limited token.
2. A small bridge runs on the LAN, ideally on Proxmox itself or in a VM/LXC.
3. The ESP32 calls that bridge over HTTP and receives simple JSON.
4. A second ESP32 can expose weather data over HTTP JSON.

For this project, PlatformIO with the Arduino framework is the most practical
choice. ESP-IDF is more professional for a polished product, but Arduino gives
fast access to stable OLED, Wi-Fi, and HTTP libraries. A later migration to
ESP-IDF would make sense for cleaner FreeRTOS usage, robust MQTT, structured
logs, advanced watchdog handling, and similar production-grade features.

## Suggested Wiring

This pinout targets the `ESP32 NODEMCU-32S ESP-32S Kit`.

Pins intentionally avoided:

- GPIO6 to GPIO11: reserved for internal flash, do not wire.
- GPIO0, GPIO2, GPIO12, GPIO15: boot strapping pins, avoid them for a stable V1.
- GPIO1 and GPIO3: USB serial port, keep them free for logs and upload.
- GPIO34 to GPIO39: input-only pins. GPIO34 is good for reading a potentiometer, but cannot drive a display.

The pins used by the project are compatible with this board:

- I2C OLED: GPIO22 SDA, GPIO23 SCL/SCK.
- Menu buttons: GPIO25, GPIO33, GPIO32.
- Red LED: GPIO27.
- Optional user potentiometer: GPIO34.

### OLED I2C 1.3"

Most 1.3" OLED modules use a 128x64 SH1106 controller.

| OLED | ESP32 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 22 |
| SCL/SCK | GPIO 23 |

If your screen uses SSD1306 instead of SH1106, only the U8g2 constructor in
`src/main.cpp` needs to change.

## OLED-Only Diagnostic Test

If the OLED stays black, first test the screen without Wi-Fi and without API
calls:

```sh
pio run -e oled-test --target upload
pio device monitor
```

Expected OLED output:

```text
OLED TEST
SDA22 SCK23
SH1106 SW I2C
```

The firmware initializes the I2C bus with `SDA=GPIO22` and `SCK/SCL=GPIO23`.

If the screen stays black with this test:

- check that OLED `SDA` is wired to `GPIO22`;
- check that OLED `SCK/SCL` is wired to `GPIO23`;
- check `VCC -> 3V3` and `GND -> GND`;
- try the other common controller if needed: some 1.3" OLED modules use `SSD1306` instead of `SH1106`;
- remember that OLED displays have no backlight: a fully black screen means it is not receiving or executing display commands.

### Menu Buttons

The three buttons control OLED pages and the pet menu:

- GPIO25: next page;
- GPIO32: previous page;
- select: open or confirm the pet menu on the home page.

| Button | ESP32 |
| --- | --- |
| Next | GPIO 25 |
| Select / OK | GPIO 33 |
| Previous | GPIO 32 |

Wire each button between its GPIO and GND. The firmware uses `INPUT_PULLUP`, so
a pressed button reads `LOW`.

Available pages:

- page 1: DigiBat home with configurable temperature/clock, current action, level, and energy/hunger/food gauges;
- page 2: network details if Proxmox is disabled, otherwise Proxmox CPU, RAM, disk, and uptime details;
- network page: Wi-Fi RSSI, ESP32 IP, latest PVE/weather data age, and OTA state;
- following pages when Proxmox is enabled: one page per VM or LXC container returned by Proxmox, with CPU, RAM, disk, and uptime.

From the home page, `Select` opens the full-screen vertical menu:

- next button: move down;
- previous button: move up;
- `Select`: confirm the selected entry;
- `STATS`: show the character sheet;
- `SLEEP`: put the companion to sleep;
- `EAT`: open the useful-meal count selector;
- `HUNT`: open the 10-minute hunt count selector;
- `ARENA`: open the automatic battle run selector to earn XP;
- `SETTINGS`: enable/disable optional integrations and choose temperature/clock on the home page;
- `SCREEN OFF`: turn the OLED off;
- `RESET PET`: open a confirmation screen before deleting and resetting the pet state;
- `BACK`: close the menu.

During an activity, `Select` opens an activity menu with `STATS`, `SCREEN OFF`,
and `BACK`. During `HUNT`, this menu also includes `STOP`. During `SLEEP`, it
also includes `WAKE UP`.

When the screen is off, any button wakes it.

The home page remains available even when Wi-Fi, Proxmox, or the weather
station does not answer. Weather and Proxmox HTTP requests run in a background
network task, so a slow local endpoint does not block DigiBat animations or
button input. Failed endpoints use a retry delay. The `SETTINGS` menu lets you
disable `WEATHER` or `PROXMOX`; when disabled, the firmware stops polling that
service. In `HOME AUTO`, the home page shows temperature when weather is enabled
and valid, otherwise it shows NTP time, or `--:--` if time is not synced yet.

Wi-Fi is mostly used for NTP time sync and OTA updates. DigiBat remains playable
without Proxmox and without a weather station; the automatic `08:00` wake-up
depends on correctly synced time.

### Red LED

The red LED is used for simple companion alerts.

| Red LED | ESP32 |
| --- | --- |
| GPIO | GPIO 27 |
| Cathode | GND |

Recommended wiring:

```text
GPIO27 -> resistance -> anode LED
cathode LED -> GND
```

A `220 ohms` resistor works. With a red LED around `2.0V`, current is roughly
`(3.3V - 2.0V) / 220 = 5.9 mA`, which is reasonable for an ESP32 GPIO.
`330 ohms` also works and will be a little softer, around `4 mA`.

Current behavior:

- the companion starts with `food = 1`, `hunger = 50`, `energy = 50`;
- in `IDLE`, hunger decreases by `2` per hour;
- in `IDLE` with no alert, the activity panel shows `chill`;
- `HUNT` lasts `10 min`, costs `2` energy, costs `0..2` hunger, and can return `0`, `1`, `2`, or `4` food;
- base `HUNT` chances are `+0 20%`, `+1 48%`, `+2 24%`, `+4 8%`;
- `ATK`, `DEF`, and `LCK` can improve hunt results;
- queued `HUNT` runs stop early when the food storage reaches `PET_MAX_FOOD`;
- `EAT` lasts `3 min`, consumes `1` food, and restores `20` hunger; during `EAT`, the `H` gauge blinks;
- during `HUNT` and `EAT`, the activity area shows the action and remaining time;
- at the end of `HUNT` and `EAT`, a modal summarizes food, hunger, and energy changes until `Select` is pressed;
- `ARENA` costs `5` energy and `5` hunger, resolves an automatic endurance fight every `2 min`, counts a win only when DigiBat survives the damage, uses `ATK`, `DEF`, `LCK`, and the current skill to reduce damage, and ends when run HP reaches `0`;
- at the end of the `ARENA` queue, a modal shows `RUNS`, `WINS`, `XP`, and `LV` until `Select` is pressed;
- `HUNT`, `EAT`, and `ARENA` queues launch one action at a time from a numeric selector;
- progression starts at `LV 1`, `XP 0/10`, `HP 10`, `ATK 2`, `DEF 1`, `LCK 1`;
- current level is shown above the `E`, `H`, and `F` gauges on the home page;
- food stock is shown on the home page as the `F` gauge;
- `SLEEP` restores `10` energy every `30 min`, decreases hunger by `3` per hour, and wakes automatically at `08:00`;
- during `SLEEP`, the `E` gauge blinks and the next energy bonus timer is shown while energy is not full;
- when temperature reaches `33.0 C`, a `HOT` notice appears for `PET_NOTICE_MS`;
- during `HOT`, the red LED turns on, then turns off when the notice ends;
- when `HUNT`, `EAT`, or `ARENA` finishes normally, the red LED plays a short non-blocking pattern;
- the heat alert can trigger again after temperature falls below `32.0 C`;
- Wi-Fi or bridge loss can also show a temporary `NET KO` notice.

### Optional User Potentiometer

If you want a user potentiometer:

| Potentiometer | ESP32 |
| --- | --- |
| end 1 | 3V3 |
| end 2 | GND |
| wiper | GPIO 34 |

The firmware already declares `USER_POT_PIN`, but does not use it yet.

## Firmware Configuration

The firmware uses `include/config.h` when it exists, otherwise it falls back to
`include/config.example.h`.

Fill in `include/config.h`:

```cpp
#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."
#define PROXMOX_METRICS_URL "http://BRIDGE_IP:8080/metrics"
#define WEATHER_URL "http://WEATHER_STATION_IP/api"
#define DASHBOARD_WEATHER_ENABLED_DEFAULT 0
#define DASHBOARD_PROXMOX_ENABLED_DEFAULT 0
#define DASHBOARD_HOME_INFO_MODE_DEFAULT 0
#define OTA_HOSTNAME "admin-dashboard"
#define OTA_PASSWORD "OTA_PASSWORD"
```

Important: `PROXMOX_METRICS_URL` is not the Proxmox web UI URL and not the raw
Proxmox API URL. It is the URL of `proxmox_bridge.py`, which then queries
Proxmox.

`DASHBOARD_WEATHER_ENABLED_DEFAULT` and `DASHBOARD_PROXMOX_ENABLED_DEFAULT`
default to `0` in the example to avoid blocking network calls for users who do
not have those services. They can later enable `WEATHER ON` or `PROXMOX ON`
from `SETTINGS`. `DASHBOARD_HOME_INFO_MODE_DEFAULT` is `0` for `AUTO`, `1` to
force temperature, or `2` to force clock.

For example, if your Proxmox server has IP `192.168.1.20` and the bridge runs
on it:

```cpp
#define PROXMOX_METRICS_URL "http://192.168.1.20:8080/metrics"
```

### DigiBat Parameters

Timing and thresholds are configurable in `include/config.h`:

```cpp
#define PET_TIMEZONE "CET-1CEST,M3.5.0/2,M10.5.0/3"
#define PET_WAKE_HOUR 8
#define PET_WAKE_MINUTE 0
#define PET_HOT_ALERT_C 33.0f
#define PET_HOT_CLEAR_C 32.0f
#define PET_NOTICE_MS 3000
#define PET_INITIAL_FOOD 1
#define PET_INITIAL_HUNGER 50
#define PET_INITIAL_ENERGY 50
#define PET_MAX_FOOD 10
#define PET_HUNT_MIN_ENERGY 2
#define PET_HUNT_ENERGY_COST 2
#define PET_HUNT_MAX_HUNGER_COST 2
#define PET_EAT_HUNGER_GAIN 20
#define PET_EAT_MAX_ENERGY_COST 5
#define PET_IDLE_HUNGER_LOSS_PER_HOUR 2
#define PET_SLEEP_ENERGY_GAIN 10
#define PET_SLEEP_ENERGY_TICK_MS (30UL * 60UL * 1000UL)
#define PET_SLEEP_HUNGER_LOSS_PER_HOUR 3
#define PET_HUNT_DURATION_MS (10UL * 60UL * 1000UL)
#define PET_EAT_DURATION_MS (3UL * 60UL * 1000UL)
#define PET_STAT_TICK_MS (60UL * 60UL * 1000UL)
#define PET_INITIAL_LEVEL 1
#define PET_INITIAL_XP 0
#define PET_INITIAL_HP 10
#define PET_INITIAL_ATK 2
#define PET_INITIAL_DEF 1
#define PET_INITIAL_LCK 1
#define PET_MAX_LEVEL 99
#define PET_ARENA_MIN_ENERGY 5
#define PET_ARENA_ENERGY_COST 5
#define PET_ARENA_HUNGER_COST 5
#define PET_ARENA_COMBAT_INTERVAL_MS (2UL * 60UL * 1000UL)
#define PET_ARENA_XP_PERCENT_AT_TARGET 35
```

The timezone above matches mainland France with daylight saving time. The ESP32
uses NTP for time-based routines.

## OTA Updates Over Wi-Fi

OTA lets you send future updates without reconnecting the ESP32 over USB. You
still need one first USB flash with OTA-capable firmware.

### 1. Configure the OTA Password

In `include/config.h`, set:

```cpp
#define OTA_HOSTNAME "admin-dashboard"
#define OTA_PASSWORD "STRONG_OTA_PASSWORD"
```

The hostname lets you reach the ESP32 through mDNS:
`admin-dashboard.local`.

### 2. First USB Flash

Plug the ESP32 over USB, then run:

```sh
pio run -e nodemcu-32s --target upload
pio device monitor
```

When Wi-Fi is connected, the serial monitor should show:

```text
OTA ready: admin-dashboard
```

### 3. Later Uploads Over Wi-Fi

The ESP32 must stay powered and connected to the same Wi-Fi network as your Mac.

Recommended command if `pio` is not in your `PATH`:

```sh
OTA_PASSWORD='STRONG_OTA_PASSWORD' /Users/aki/.platformio/penv/bin/pio run -e nodemcu-32s-ota --target upload
```

If `pio` works directly in your terminal, use the shorter version:

```sh
OTA_PASSWORD='STRONG_OTA_PASSWORD' pio run -e nodemcu-32s-ota --target upload
```

You can also export the password once in the current terminal:

```sh
export OTA_PASSWORD='STRONG_OTA_PASSWORD'
/Users/aki/.platformio/penv/bin/pio run -e nodemcu-32s-ota --target upload
```

If `admin-dashboard.local` does not resolve on your network, temporarily
replace `upload_port` in `platformio.ini` with the ESP32 IP address:

```ini
upload_port = 192.168.1.xx
```

If upload stays stuck on `Sending invitation to 192.168.1.xx`, PlatformIO is
starting the transfer but the ESP32 is not answering OTA yet. Check in this
order:

- the firmware currently flashed on the ESP32 includes OTA; the first OTA-capable
  firmware must be sent over USB;
- the serial monitor shows `OTA ready: admin-dashboard` after Wi-Fi connects;
- without USB, open the OLED `NETWORK` page and check the `OTA:` line:
  `ON :3232` means the OTA service is listening, `OFF WIFI` means Wi-Fi is not
  connected, and `OFF INIT` means OTA has not initialized yet;
- the IP address is the dashboard ESP32 IP, not the weather ESP32 or bridge IP;
- your Mac and ESP32 are on the same Wi-Fi/VLAN, without client isolation;
- the terminal `OTA_PASSWORD` matches the password already flashed in firmware.

For stable use, reserve the ESP32 IP address in your router.

## Expected JSON Format

Bridge Proxmox:

```json
{
  "cpu": 12.3,
  "ram": 45.6,
  "ram_used_gb": 7.3,
  "ram_total_gb": 16.0,
  "storage": 78.9,
  "storage_used_gb": 120.5,
  "storage_total_gb": 512.0,
  "uptime": 123456,
  "guests": [
    {
      "type": "lxc",
      "vmid": 101,
      "name": "ai-buddy-hub",
      "status": "running",
      "cpu": 3.2,
      "ram": 42.0,
      "ram_used_gb": 0.4,
      "ram_total_gb": 1.0,
      "storage": 22.5,
      "storage_used_gb": 1.8,
      "storage_total_gb": 8.0,
      "uptime": 123456
    }
  ]
}
```

`ram` and `storage` remain percentages. `*_gb` fields provide real quantities
in GiB. `guests` contains VMs and LXC containers, limited to `10` entries by
default on the bridge side and `6` maximum pages on the ESP32 side.

Weather ESP32:

```json
{"temperature":31.9,"humidity":45,"status":"OPTIMAL","datetime":"19:30 - 02/07/2026","ip":"192.168.1.30"}
```

If your weather station IP opens an HTML page, that is normal. The firmware
must not call the root page; it must call the JSON API:

```cpp
#define WEATHER_URL "http://192.168.1.30/api"
```

## Bridge Proxmox

The `bridge/proxmox_bridge.py` script uses only the Python standard library.

You can run it directly on Proxmox, in a VM, in an LXC, or on another always-on
machine. The simplest setup is usually the Proxmox server itself or a small LXC.

Example when the bridge runs on the Proxmox server itself:

Environment variables:

```sh
export PVE_HOST="https://127.0.0.1:8006"
export PVE_NODE="pve"
export PVE_TOKEN_ID='root@pam!esp32-dashboard'
export PVE_TOKEN_SECRET='TOKEN_SECRET'
export PVE_VERIFY_SSL="false"
python3 bridge/proxmox_bridge.py
```

Endpoint:

```sh
curl http://PROXMOX_IP:8080/metrics
```

The ESP32 must call this simple HTTP URL:

```cpp
#define PROXMOX_METRICS_URL "http://PROXMOX_IP:8080/metrics"
```

The bridge calls the Proxmox API:

```text
https://127.0.0.1:8006/api2/json/nodes/pve/status
```

There are two separate flows:

- ESP32 -> bridge: `http://PROXMOX_IP:8080/metrics`
- bridge -> Proxmox API: `https://127.0.0.1:8006/...`

## Build and Upload

With PlatformIO:

```sh
pio run
pio run --target upload
pio device monitor
```

The project targets `board = nodemcu-32s` in `platformio.ini`.

## Recommended Next Step

For a more robust V2, replace weather HTTP polling with MQTT:

- the second ESP32 publishes `home/weather/livingroom`;
- the dashboard subscribes to that topic;
- Proxmox can stay on the HTTP bridge or publish through MQTT too.

Keep the Proxmox token on the server side, never in ESP32 firmware.
