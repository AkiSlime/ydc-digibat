# Your Pet Desk: DigiBat Firmware User Guide

This firmware turns an ESP32 with a small OLED screen into a desk dashboard and
a tiny virtual pet companion.

It has two main roles:

- show quick local status information, such as Proxmox, network, and weather
  data;
- run DigiBat, a pixel-art bat with energy, hunger, food, levels, and actions.

The home page keeps working even when Wi-Fi, the weather station, or the
Proxmox bridge is unavailable. Some dashboard information may be missing, but
the pet remains playable.

## Controls

The firmware uses three buttons:

- `Next`: go to the next page or move down in a menu;
- `Previous`: go to the previous page or move up in a menu;
- `Select`: open a menu, confirm a choice, or close some screens.

When the OLED is off, any button wakes it.

## Main Pages

### DigiBat Home

The first page is the main page. It shows:

- temperature or clock, depending on the selected setting;
- the current pet activity or status;
- the animated bat sprite;
- the current level;
- the `E`, `H`, and `F` gauges.

Gauge meanings:

- `E`: energy;
- `H`: hunger;
- `F`: food stock.

When everything is fine and no action is running, the activity panel shows
`chill`.

### Proxmox Page

The Proxmox page appears only when Proxmox is enabled in `SETTINGS`. It shows a
summary of the monitored machine:

- CPU;
- RAM;
- disk;
- uptime.

This data comes from the Proxmox bridge configured on your local network.

### Network Page

The network page shows useful diagnostic information:

- Wi-Fi signal;
- ESP32 IP address;
- age of the latest Proxmox and weather data;
- OTA service status.

This page is useful to confirm that the ESP32 is connected and ready for Wi-Fi
updates.

### VM and LXC Pages

When Proxmox is enabled, the firmware can display one page per VM or LXC
container returned by the bridge.

Each page shows the name, state, CPU, RAM, disk, and uptime for that guest.

## DigiBat

DigiBat is a pixel-art bat. It does not choose actions by itself: you watch its
stats and decide what to do next.

Its main needs are:

- energy: required for hunting and arena runs;
- hunger: decreases over time and during some actions;
- food: used to eat and restore hunger.

DigiBat also has RPG-style progression:

- level;
- XP;
- HP;
- ATK;
- DEF;
- LCK;
- best arena score;
- title;
- unlocked skill.

Progression stats mainly affect hunting and arena performance.

## Pet Menu

From the home page, press `Select` to open the main menu.

The menu contains:

- `STATS`: view DigiBat's character sheet;
- `SLEEP`: recover energy;
- `EAT`: restore hunger;
- `HUNT`: hunt for food;
- `ARENA`: run automatic battles to earn XP;
- `SETTINGS`: configure optional integrations and the home display;
- `SCREEN OFF`: turn the OLED off;
- `RESET PET`: delete and reset the pet state after confirmation;
- `BACK`: close the menu.

The menu is vertical. `Next` moves down, `Previous` moves up, and `Select`
confirms the highlighted item.

## SETTINGS

`SETTINGS` makes the firmware usable on a simple setup, without a weather
station or Proxmox server.

Available settings:

- `WEATHER ON/OFF`: enable or disable calls to the weather station;
- `PROXMOX ON/OFF`: enable or disable calls to the Proxmox bridge;
- `HOME AUTO/TEMP/CLOCK`: choose what appears in the top-right corner of the home page;
- `BACK`: return to the main menu.

In `HOME AUTO`, the home page shows temperature when `WEATHER` is enabled and
valid. Otherwise it shows the NTP-synced clock. If time is not available yet,
it shows `--:--`.

When `WEATHER` or `PROXMOX` is `OFF`, the firmware stops polling that service.
This avoids slowdowns caused by local URLs that do not answer.

Wi-Fi is still useful for time sync and OTA updates. DigiBat remains playable
without Proxmox and without a weather station. The automatic 08:00 wake-up
depends on synced time.

## STATS Sheet

`STATS` opens a multi-page character sheet.

It shows:

- level, XP, and combat stats;
- current needs: energy, hunger, and food;
- a quick reminder for `E`, `H`, and `F`;
- current title and skill.

In this view, `Next` and `Previous` change pages. `Select` closes the sheet.

## Actions

### HUNT

`HUNT` starts a hunt.

A hunt:

- lasts 20 minutes;
- costs energy;
- may reduce hunger;
- may earn food.

Base results:

- `+0` food: 20%;
- `+1` food: 48%;
- `+2` food: 24%;
- `+4` food: 8%.

`ATK`, `DEF`, and `LCK` can improve the result or reduce negative effects.

Before starting `HUNT`, the firmware opens a quantity selector. It lets you
choose how many hunts to queue based on available energy.

During a hunt, `Select` opens the activity menu. `STOP` cancels the current
hunt. A stopped hunt still pays its costs, but does not earn food.

At the end of one or more hunts, a `HUNT END` window shows the number of runs
and the food, hunger, and energy changes. Press `Select` to close it.

### EAT

`EAT` lets DigiBat eat.

A meal:

- lasts 3 minutes;
- consumes 1 food;
- restores hunger;
- blinks the hunger gauge during the action;
- may cost a little energy.

The firmware avoids wasting food: if hunger is already full, eating is blocked.

Like hunting, eating uses a quantity selector when multiple useful meals are
available.

At the end of one or more meals, an `EAT END` window shows the number of runs
and the food, hunger, and energy changes. Press `Select` to close it.

### SLEEP

`SLEEP` puts DigiBat to sleep.

During sleep:

- energy increases by 10 every 30 minutes;
- hunger slowly decreases;
- the remaining time before the next energy bonus is shown while energy is not full;
- the energy gauge blinks while energy is recovering;
- DigiBat can wake automatically at 08:00;
- you can also wake it manually with `WAKE UP`.

Sleep is not a timed action like `HUNT` or `EAT`. It lasts until manual or
automatic wake-up.

### ARENA

`ARENA` starts automatic battles.

An arena run:

- costs energy and hunger at launch;
- resolves one automatic fight every 2 minutes;
- shows the current win count;
- plays a random animation between attacks and hurt reactions;
- ends when the run HP reaches 0;
- grants XP based on the result.

At the end of the arena queue, a single summary window shows:

- `RUNS`;
- `WINS`;
- `XP`;
- `LV`.

This window stays visible until you press `Select`.

If several arena runs were selected, they chain automatically. You only see the
final total summary.

## Alerts and Messages

The activity panel can show short messages:

- `NET KO`: Wi-Fi issue or unavailable Proxmox bridge while the integration is enabled;
- `HOT`: temperature above the configured threshold;
- `NO ENERGY`: not enough energy;
- `NO FOOD`: no food left;
- `FULL`: hunger already full;
- `BUSY`: another action is blocking the request;
- `STARVING`: hunger is very low;
- `TIRED`: energy is low.

These messages help explain why an action is available or blocked.

## Red LED

The red LED has two roles:

- signal a heat alert with `HOT`;
- play a short completion pattern when `HUNT`, `EAT`, or `ARENA` finishes
  normally.

The completion pattern is non-blocking. The dashboard continues running while
the LED signal plays.

## OTA Updates

After the first USB flash, the firmware can be updated over Wi-Fi with OTA.

The network page shows whether OTA is ready:

- `ON :3232`: OTA service available;
- `OFF WIFI`: Wi-Fi not connected;
- `OFF INIT`: OTA not initialized yet.

OTA does not change daily use. It is only a more convenient way to send a new
firmware version.

## Important Notes

- DigiBat has no death, sickness, or care mechanic in this version.
- It does not hunt, eat, or sleep by itself.
- The player keeps control of all actions.
- Food is limited by a maximum stock.
- Values can be adjusted in the firmware configuration.
- If Proxmox or weather does not answer, the DigiBat home page remains usable.
- Proxmox and weather can be disabled in `SETTINGS`.
