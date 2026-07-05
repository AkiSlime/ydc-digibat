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

// Tamagotchi routines. Times use the ESP32 local timezone configured below.
#define PET_TIMEZONE "CET-1CEST,M3.5.0/2,M10.5.0/3"
#define PET_NTP_SERVER_1 "pool.ntp.org"
#define PET_NTP_SERVER_2 "time.nist.gov"
#define PET_FEED_HOUR 13
#define PET_FEED_MINUTE 0
#define PET_SLEEP_HOUR 22
#define PET_SLEEP_MINUTE 30
#define PET_WAKE_HOUR 8
#define PET_WAKE_MINUTE 0
#define PET_HOT_ALERT_C 30.0f
#define PET_HOT_CLEAR_C 29.0f

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
