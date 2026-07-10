#pragma once

// Duplicate this file to include/config.h and fill in your real values.
// include/config.h is ignored by git so Wi-Fi credentials stay local.

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Real setup: point this at the bridge that exposes /metrics, not at the raw Proxmox API.
// If the bridge runs on your Proxmox server at 192.168.1.20, use:
// http://192.168.1.20:8080/metrics
// Expected JSON: {"cpu":12.3,"ram":45.6,"storage":78.9,"guests":[{"type":"lxc","vmid":101,"name":"hub","cpu":3.2,"ram":42.0}]}
#define PROXMOX_METRICS_URL "http://192.168.1.10:8080/metrics"

// Expected JSON from the second ESP32 API:
// {"temperature":31.9,"humidity":45,"status":"OPTIMAL","datetime":"19:30 - 02/07/2026","ip":"192.168.1.30"}
#define WEATHER_URL "http://192.168.1.30/api"

#define HTTP_TIMEOUT_MS 2500
#define PROXMOX_REFRESH_MS 5000
#define WEATHER_REFRESH_MS 10000

// Optional integrations. They are disabled by default for public installs.
// Enable them from the OLED SETTINGS menu, or set these defaults to 1.
#define DASHBOARD_WEATHER_ENABLED_DEFAULT 0
#define DASHBOARD_PROXMOX_ENABLED_DEFAULT 0
// Home info mode: 0 = auto, 1 = temperature, 2 = clock.
#define DASHBOARD_HOME_INFO_MODE_DEFAULT 0

// Tamagotchi routines. Times use the ESP32 local timezone configured below.
#define PET_TIMEZONE "CET-1CEST,M3.5.0/2,M10.5.0/3"
#define PET_NTP_SERVER_1 "pool.ntp.org"
#define PET_NTP_SERVER_2 "time.nist.gov"
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

// OTA updates over Wi-Fi. Flash once over USB, then use the `nodemcu-32s-ota`
// PlatformIO environment while the ESP32 is powered on and connected to Wi-Fi.
#define OTA_HOSTNAME "admin-dashboard"
#define OTA_PASSWORD "CHANGE_ME_STRONG_OTA_PASSWORD"

// ESP32 NodeMCU-32S / ESP-32S Kit pinout.
// Keep GPIO6-GPIO11 free: they are wired to the onboard flash.
// Avoid GPIO0, GPIO2, GPIO12 and GPIO15 for peripherals unless you know the boot strapping impact.
// Avoid GPIO1/GPIO3 for the dashboard wiring because they are used by USB serial logs.

// OLED 1.3" I2C, usually SH1106 128x64.
#define OLED_SDA_PIN 22
#define OLED_SCL_PIN 23 // SCL, sometimes labeled SCK on OLED modules.

// Menu buttons. Wire each button between GPIO and GND.
// The firmware uses INPUT_PULLUP, so pressed = LOW.
#define BUTTON_LEFT_PIN 25
#define BUTTON_SELECT_PIN 33
#define BUTTON_RIGHT_PIN 32

// Red status LED. Wire GPIO -> resistor -> LED anode, LED cathode -> GND.
#define RED_LED_PIN 27

// Optional user potentiometer input.
#define USER_POT_PIN 34
