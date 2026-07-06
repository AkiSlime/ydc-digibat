#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

#include "tamagotchi_bat_sprites.h"

#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(
    U8G2_R0,
    U8X8_PIN_NONE);

const uint8_t MAX_GUESTS = 6;
const uint8_t BASE_PAGE_COUNT = 3;

#ifndef PET_TIMEZONE
#define PET_TIMEZONE "CET-1CEST,M3.5.0/2,M10.5.0/3"
#endif

#ifndef PET_NTP_SERVER_1
#define PET_NTP_SERVER_1 "pool.ntp.org"
#endif

#ifndef PET_NTP_SERVER_2
#define PET_NTP_SERVER_2 "time.nist.gov"
#endif

#ifndef PET_FEED_HOUR
#define PET_FEED_HOUR 12
#endif

#ifndef PET_FEED_MINUTE
#define PET_FEED_MINUTE 0
#endif

#ifndef PET_AFTERNOON_WORK_HOUR
#define PET_AFTERNOON_WORK_HOUR 14
#endif

#ifndef PET_AFTERNOON_WORK_MINUTE
#define PET_AFTERNOON_WORK_MINUTE 0
#endif

#ifndef PET_CHILL_HOUR
#define PET_CHILL_HOUR 18
#endif

#ifndef PET_CHILL_MINUTE
#define PET_CHILL_MINUTE 0
#endif

#ifndef PET_SLEEP_HOUR
#define PET_SLEEP_HOUR 23
#endif

#ifndef PET_SLEEP_MINUTE
#define PET_SLEEP_MINUTE 30
#endif

#ifndef PET_WAKE_HOUR
#define PET_WAKE_HOUR 8
#endif

#ifndef PET_WAKE_MINUTE
#define PET_WAKE_MINUTE 0
#endif

#ifndef PET_HOT_ALERT_C
#define PET_HOT_ALERT_C 33.0f
#endif

#ifndef PET_HOT_CLEAR_C
#define PET_HOT_CLEAR_C 32.0f
#endif

#ifndef PET_NOTICE_MS
#define PET_NOTICE_MS 3000
#endif

#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "admin-dashboard"
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "admin-dashboard-ota"
#endif

struct GuestMetrics {
  String type = "";
  uint16_t vmid = 0;
  String name = "";
  String status = "";
  float cpu = NAN;
  float ram = NAN;
  float ramUsedGb = NAN;
  float ramTotalGb = NAN;
  float storage = NAN;
  float storageUsedGb = NAN;
  float storageTotalGb = NAN;
  uint32_t uptime = 0;
};

struct ProxmoxMetrics {
  float cpu = NAN;
  float ram = NAN;
  float ramUsedGb = NAN;
  float ramTotalGb = NAN;
  float storage = NAN;
  float storageUsedGb = NAN;
  float storageTotalGb = NAN;
  uint32_t uptime = 0;
  GuestMetrics guests[MAX_GUESTS];
  uint8_t guestCount = 0;
  bool valid = false;
  String error = "waiting";
  uint32_t lastOkMs = 0;
};

struct WeatherMetrics {
  float temperature = NAN;
  float humidity = NAN;
  String status = "";
  String datetime = "";
  String ip = "";
  bool valid = false;
  String error = "waiting";
  uint32_t lastOkMs = 0;
};

ProxmoxMetrics proxmox;
WeatherMetrics weather;
Preferences petPrefs;

const char *FIRMWARE_VERSION = "OLED v15 PET";

enum DashboardPage : uint8_t {
  PAGE_DASHBOARD = 0,
  PAGE_PROXMOX = 1,
  PAGE_NETWORK = 2,
};

enum PetMenuAction : uint8_t {
  PET_ACTION_FEED = 0,
  PET_ACTION_SLEEP = 1,
  PET_ACTION_PAGES = 2,
  PET_ACTION_SCREEN_OFF = 3,
  PET_ACTION_BACK = 4,
  PET_ACTION_COUNT = 5,
};

enum PetBaseMode : uint8_t {
  PET_BASE_SLEEP = 0,
  PET_BASE_WORK = 1,
  PET_BASE_HUNGRY = 2,
  PET_BASE_CHILL = 3,
};

enum PetNoticeType : uint8_t {
  PET_NOTICE_NONE = 0,
  PET_NOTICE_HOT = 1,
  PET_NOTICE_NETWORK = 2,
};

struct PetRenderSprite {
  const uint8_t *bitmap = nullptr;
  uint8_t x = 0;
  bool flipX = false;
};

struct PetState {
  bool asleep = false;
  bool sleepUntilMorning = false;
  bool hotLatched = false;
  bool connectionAlertLatched = false;
  PetNoticeType notice = PET_NOTICE_NONE;
  uint32_t noticeUntilMs = 0;
  int32_t fedDay = -1;
  int32_t sleepStartDay = -1;
};

uint8_t currentPage = PAGE_DASHBOARD;
bool displayEnabled = true;
bool petMenuOpen = false;
uint8_t petMenuIndex = PET_ACTION_FEED;
bool timeConfigured = false;
bool otaConfigured = false;
String otaUiStatus = "init";
uint8_t otaUiProgress = 0;

uint32_t lastProxmoxPollMs = 0;
uint32_t lastWeatherPollMs = 0;
uint32_t lastWifiAttemptMs = 0;
uint32_t weatherNoticeUntilMs = 0;
uint32_t lastLeftPressMs = 0;
uint32_t lastSelectPressMs = 0;
uint32_t lastRightPressMs = 0;

volatile bool leftButtonPending = false;
volatile bool selectButtonPending = false;
volatile bool rightButtonPending = false;

const uint32_t BUTTON_LOCKOUT_MS = 120;
const uint32_t WEATHER_NOTICE_MS = 5000;
const uint16_t PET_ANIMATION_FRAME_MS = 180;
const uint16_t PET_IDLE_MS = 3500;
const uint16_t PET_MOVE_MS = 1800;
const uint8_t PET_RENDER_DELAY_MS = 20;
const uint8_t PET_SCALE_NUM = 2;
const uint8_t PET_SCALE_DEN = 1;
const uint8_t PET_X_LEFT = 0;
const uint8_t PET_X_RIGHT = 24;
const uint8_t PET_Y = 16;
PetState pet;

void IRAM_ATTR onLeftButtonFall() {
  leftButtonPending = true;
}

void IRAM_ATTR onSelectButtonFall() {
  selectButtonPending = true;
}

void IRAM_ATTR onRightButtonFall() {
  rightButtonPending = true;
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  const uint32_t now = millis();
  if (lastWifiAttemptMs != 0 && now - lastWifiAttemptMs < 10000) {
    return;
  }

  lastWifiAttemptMs = now;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool getJson(const char *url, JsonDocument &doc, String &error) {
  if (WiFi.status() != WL_CONNECTED) {
    error = "wifi";
    return false;
  }

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);

  if (!http.begin(url)) {
    error = "url";
    return false;
  }

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    error = "http " + String(status);
    http.end();
    return false;
  }

  DeserializationError jsonError = deserializeJson(doc, http.getStream());
  http.end();

  if (jsonError) {
    error = "json";
    return false;
  }

  return true;
}

void pollProxmox() {
  JsonDocument doc;
  String error;

  if (!getJson(PROXMOX_METRICS_URL, doc, error)) {
    proxmox.valid = false;
    proxmox.error = error;
    return;
  }

  proxmox.cpu = doc["cpu"] | NAN;
  proxmox.ram = doc["ram"] | NAN;
  proxmox.ramUsedGb = doc["ram_used_gb"] | NAN;
  proxmox.ramTotalGb = doc["ram_total_gb"] | NAN;
  proxmox.storage = doc["storage"] | NAN;
  proxmox.storageUsedGb = doc["storage_used_gb"] | NAN;
  proxmox.storageTotalGb = doc["storage_total_gb"] | NAN;
  proxmox.uptime = doc["uptime"] | 0;
  proxmox.guestCount = 0;

  JsonArray guests = doc["guests"].as<JsonArray>();
  if (!guests.isNull()) {
    for (JsonObject guest : guests) {
      if (proxmox.guestCount >= MAX_GUESTS) {
        break;
      }

      GuestMetrics &target = proxmox.guests[proxmox.guestCount++];
      target.type = guest["type"] | "";
      target.vmid = guest["vmid"] | 0;
      target.name = guest["name"] | "";
      target.status = guest["status"] | "";
      target.cpu = guest["cpu"] | NAN;
      target.ram = guest["ram"] | NAN;
      target.ramUsedGb = guest["ram_used_gb"] | NAN;
      target.ramTotalGb = guest["ram_total_gb"] | NAN;
      target.storage = guest["storage"] | NAN;
      target.storageUsedGb = guest["storage_used_gb"] | NAN;
      target.storageTotalGb = guest["storage_total_gb"] | NAN;
      target.uptime = guest["uptime"] | 0;
    }
  }

  if (isnan(proxmox.cpu) || isnan(proxmox.ram) || isnan(proxmox.storage)) {
    proxmox.valid = false;
    proxmox.error = "fields";
    return;
  }

  proxmox.valid = true;
  proxmox.error = "";
  proxmox.lastOkMs = millis();
}

void pollWeather() {
  JsonDocument doc;
  String error;
  const bool wasValid = weather.valid;

  if (!getJson(WEATHER_URL, doc, error)) {
    weather.valid = false;
    weather.error = error;
    if (wasValid) {
      weatherNoticeUntilMs = millis() + WEATHER_NOTICE_MS;
      Serial.print("Weather disconnected: ");
      Serial.println(error);
    }
    return;
  }

  weather.temperature = doc["temperature"] | NAN;
  weather.humidity = doc["humidity"] | NAN;
  weather.status = doc["status"] | "";
  weather.datetime = doc["datetime"] | "";
  weather.ip = doc["ip"] | "";

  if (isnan(weather.temperature) || isnan(weather.humidity)) {
    weather.valid = false;
    weather.error = "fields";
    if (wasValid) {
      weatherNoticeUntilMs = millis() + WEATHER_NOTICE_MS;
      Serial.println("Weather disconnected: fields");
    }
    return;
  }

  weather.valid = true;
  weather.error = "";
  weatherNoticeUntilMs = 0;
  weather.lastOkMs = millis();
}

String weatherTime(const String &value) {
  const int separator = value.indexOf(" - ");
  if (separator > 0) {
    return value.substring(0, separator);
  }

  const int space = value.indexOf(' ');
  if (space > 0) {
    return value.substring(0, space);
  }

  return value;
}

String weatherDateShort(const String &value) {
  const int separator = value.indexOf(" - ");
  const String date = separator >= 0 ? value.substring(separator + 3) : "";

  if (date.length() == 10 && date.charAt(2) == '/' && date.charAt(5) == '/') {
    return date;
  }

  return "";
}

String formatPercent(float value) {
  if (isnan(value)) {
    return "?";
  }

  return String(lround(value));
}

String formatPercentUnit(float value) {
  return formatPercent(value) + " %";
}

String formatGb(float value) {
  if (isnan(value)) {
    return "?GB";
  }

  return String(value, 1) + "GB";
}

String formatUptime(uint32_t seconds) {
  const uint32_t days = seconds / 86400;
  const uint8_t hours = (seconds % 86400) / 3600;
  const uint8_t minutes = (seconds % 3600) / 60;

  if (days > 0) {
    return String(days) + "d " + String(hours) + "h";
  }

  if (hours > 0) {
    return String(hours) + "h " + String(minutes) + "m";
  }

  return String(minutes) + "m";
}

String formatAge(uint32_t lastOkMs, bool valid) {
  if (!valid || lastOkMs == 0) {
    return "ko";
  }

  const uint32_t ageSeconds = (millis() - lastOkMs) / 1000;
  if (ageSeconds < 60) {
    return String(ageSeconds) + "s";
  }

  return String(ageSeconds / 60) + "m";
}

String truncateText(const String &value, uint8_t maxChars) {
  if (value.length() <= maxChars) {
    return value;
  }

  if (maxChars <= 1) {
    return value.substring(0, maxChars);
  }

  return value.substring(0, maxChars - 1) + ".";
}

bool readLocalTime(struct tm &timeInfo) {
  return getLocalTime(&timeInfo, 0);
}

int32_t dayKey(const struct tm &timeInfo) {
  return static_cast<int32_t>(timeInfo.tm_year + 1900) * 1000 + timeInfo.tm_yday;
}

uint16_t minutesSinceMidnight(const struct tm &timeInfo) {
  return static_cast<uint16_t>(timeInfo.tm_hour * 60 + timeInfo.tm_min);
}

uint16_t configuredMinutes(uint8_t hour, uint8_t minute) {
  return static_cast<uint16_t>(hour * 60 + minute);
}

bool isSleepWindow(uint16_t nowMinutes) {
  const uint16_t sleepMinutes = configuredMinutes(PET_SLEEP_HOUR, PET_SLEEP_MINUTE);
  const uint16_t wakeMinutes = configuredMinutes(PET_WAKE_HOUR, PET_WAKE_MINUTE);

  if (sleepMinutes > wakeMinutes) {
    return nowMinutes >= sleepMinutes || nowMinutes < wakeMinutes;
  }

  return nowMinutes >= sleepMinutes && nowMinutes < wakeMinutes;
}

String formatClockFromNtp() {
  struct tm timeInfo;
  if (!readLocalTime(timeInfo)) {
    return "";
  }

  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
  return String(buffer);
}

void configureTimeIfNeeded() {
  if (timeConfigured || WiFi.status() != WL_CONNECTED) {
    return;
  }

  configTzTime(PET_TIMEZONE, PET_NTP_SERVER_1, PET_NTP_SERVER_2);
  timeConfigured = true;
  Serial.println("NTP configured");
}

void configureOtaIfNeeded() {
  if (otaConfigured || WiFi.status() != WL_CONNECTED) {
    if (WiFi.status() != WL_CONNECTED) {
      otaUiStatus = "wifi";
    }
    return;
  }

  ArduinoOTA.setPort(3232);
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA
      .onStart([]() {
        otaUiStatus = "start";
        otaUiProgress = 0;
        Serial.println("OTA start");
      })
      .onEnd([]() {
        otaUiStatus = "done";
        otaUiProgress = 100;
        Serial.println("OTA end");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        otaUiProgress = (progress * 100) / total;
        otaUiStatus = String(otaUiProgress) + "%";
        Serial.printf("OTA progress: %u%%\r", otaUiProgress);
      })
      .onError([](ota_error_t error) {
        switch (error) {
        case OTA_AUTH_ERROR:
          otaUiStatus = "err auth";
          break;
        case OTA_BEGIN_ERROR:
          otaUiStatus = "err begin";
          break;
        case OTA_CONNECT_ERROR:
          otaUiStatus = "err conn";
          break;
        case OTA_RECEIVE_ERROR:
          otaUiStatus = "err recv";
          break;
        case OTA_END_ERROR:
          otaUiStatus = "err end";
          break;
        default:
          otaUiStatus = "err";
          break;
        }
        Serial.printf("OTA error[%u]\n", error);
      });

  ArduinoOTA.begin();
  otaConfigured = true;
  otaUiStatus = "ready";
  Serial.print("OTA ready: ");
  Serial.println(OTA_HOSTNAME);
}

void loadPetState() {
  pet.asleep = petPrefs.getBool("asleep", false);
  pet.sleepUntilMorning = petPrefs.getBool("sleepWake", false);
  pet.fedDay = petPrefs.getInt("fedDay", -1);
  pet.sleepStartDay = petPrefs.getInt("sleepDay", -1);
  displayEnabled = petPrefs.getBool("display", true);
}

void savePetState() {
  petPrefs.putBool("asleep", pet.asleep);
  petPrefs.putBool("sleepWake", pet.sleepUntilMorning);
  petPrefs.putInt("fedDay", pet.fedDay);
  petPrefs.putInt("sleepDay", pet.sleepStartDay);
  petPrefs.putBool("display", displayEnabled);
}

bool hasConnectionAlert() {
  return WiFi.status() != WL_CONNECTED || !proxmox.valid;
}

bool isLunchWindow(const struct tm &timeInfo) {
  const uint16_t nowMinutes = minutesSinceMidnight(timeInfo);
  const uint16_t feedMinutes = configuredMinutes(PET_FEED_HOUR, PET_FEED_MINUTE);
  const uint16_t afternoonWorkMinutes = configuredMinutes(PET_AFTERNOON_WORK_HOUR, PET_AFTERNOON_WORK_MINUTE);
  return nowMinutes >= feedMinutes && nowMinutes < afternoonWorkMinutes;
}

bool hasMealPrompt(const struct tm &timeInfo) {
  return isLunchWindow(timeInfo) && pet.fedDay != dayKey(timeInfo);
}

PetBaseMode currentPetBaseMode() {
  struct tm timeInfo;
  if (!readLocalTime(timeInfo)) {
    return pet.asleep ? PET_BASE_SLEEP : PET_BASE_WORK;
  }

  const uint16_t nowMinutes = minutesSinceMidnight(timeInfo);
  const uint16_t chillMinutes = configuredMinutes(PET_CHILL_HOUR, PET_CHILL_MINUTE);

  if (pet.asleep || isSleepWindow(nowMinutes)) {
    return PET_BASE_SLEEP;
  }

  if (hasMealPrompt(timeInfo)) {
    return PET_BASE_HUNGRY;
  }

  if (nowMinutes >= chillMinutes) {
    return PET_BASE_CHILL;
  }

  return PET_BASE_WORK;
}

bool hasMealAlert() {
  return currentPetBaseMode() == PET_BASE_HUNGRY;
}

PetNoticeType activePetNotice() {
  if (pet.notice == PET_NOTICE_NONE || static_cast<int32_t>(millis() - pet.noticeUntilMs) >= 0) {
    return PET_NOTICE_NONE;
  }

  return pet.notice;
}

void startPetNotice(PetNoticeType notice) {
  pet.notice = notice;
  pet.noticeUntilMs = millis() + PET_NOTICE_MS;
}

void updatePetRoutine() {
  bool changed = false;

  if (weather.valid && !isnan(weather.temperature)) {
    if (!pet.hotLatched && weather.temperature >= PET_HOT_ALERT_C) {
      pet.hotLatched = true;
      startPetNotice(PET_NOTICE_HOT);
    } else if (pet.hotLatched && weather.temperature < PET_HOT_CLEAR_C) {
      pet.hotLatched = false;
    }
  }

  const bool connectionAlert = hasConnectionAlert();
  if (connectionAlert && !pet.connectionAlertLatched) {
    startPetNotice(PET_NOTICE_NETWORK);
  }
  pet.connectionAlertLatched = connectionAlert;

  struct tm timeInfo;
  if (readLocalTime(timeInfo)) {
    const int32_t today = dayKey(timeInfo);
    const uint16_t nowMinutes = minutesSinceMidnight(timeInfo);
    const bool sleepWindow = isSleepWindow(nowMinutes);

    if (sleepWindow && !pet.asleep) {
      pet.asleep = true;
      pet.sleepUntilMorning = false;
      pet.sleepStartDay = today;
      changed = true;
    } else if (!sleepWindow && pet.asleep) {
      const uint16_t wakeMinutes = configuredMinutes(PET_WAKE_HOUR, PET_WAKE_MINUTE);
      const bool morningReached = nowMinutes >= wakeMinutes;
      const bool sleptAcrossDay = pet.sleepStartDay < 0 || pet.sleepStartDay != today;

      if (!pet.sleepUntilMorning || (morningReached && sleptAcrossDay)) {
        pet.asleep = false;
        pet.sleepUntilMorning = false;
        changed = true;
      }
    }
  }

  if (changed) {
    savePetState();
  }
}

void updateAlertLed() {
  digitalWrite(RED_LED_PIN, activePetNotice() == PET_NOTICE_HOT ? HIGH : LOW);
}

const char *petMenuLabel(uint8_t index) {
  switch (index) {
  case PET_ACTION_FEED:
    return "NOURRIR";
  case PET_ACTION_SLEEP:
    return pet.asleep ? "REVEIL" : "DODO";
  case PET_ACTION_PAGES:
    return "PAGES";
  case PET_ACTION_SCREEN_OFF:
    return "ECRAN OFF";
  case PET_ACTION_BACK:
    return "RETOUR";
  default:
    return "";
  }
}

void executePetAction() {
  struct tm timeInfo;
  const bool hasTime = readLocalTime(timeInfo);

  switch (petMenuIndex) {
  case PET_ACTION_FEED:
    if (hasTime) {
      pet.fedDay = dayKey(timeInfo);
    }
    petMenuOpen = false;
    savePetState();
    break;
  case PET_ACTION_SLEEP:
    pet.asleep = !pet.asleep;
    pet.sleepUntilMorning = pet.asleep;
    pet.sleepStartDay = hasTime ? dayKey(timeInfo) : -1;
    petMenuOpen = false;
    savePetState();
    break;
  case PET_ACTION_PAGES:
    petMenuOpen = false;
    currentPage = PAGE_PROXMOX;
    break;
  case PET_ACTION_SCREEN_OFF:
    petMenuOpen = false;
    displayEnabled = false;
    oled.setPowerSave(1);
    savePetState();
    break;
  case PET_ACTION_BACK:
    petMenuOpen = false;
    break;
  default:
    break;
  }
}

uint8_t pageCount() {
  return BASE_PAGE_COUNT + proxmox.guestCount;
}

bool consumeButtonPress(volatile bool &pending, uint32_t &lastPressMs) {
  bool pressed = false;

  noInterrupts();
  if (pending) {
    pending = false;
    pressed = true;
  }
  interrupts();

  if (!pressed) {
    return false;
  }

  const uint32_t now = millis();
  if (now - lastPressMs < BUTTON_LOCKOUT_MS) {
    return false;
  }

  lastPressMs = now;
  return true;
}

void previousPage() {
  if (currentPage == PAGE_DASHBOARD) {
    currentPage = pageCount() - 1;
    return;
  }

  currentPage--;
}

void nextPage() {
  currentPage = (currentPage + 1) % pageCount();
}

void handleButtons() {
  const bool leftPressed = consumeButtonPress(leftButtonPending, lastLeftPressMs);
  const bool selectPressed = consumeButtonPress(selectButtonPending, lastSelectPressMs);
  const bool rightPressed = consumeButtonPress(rightButtonPending, lastRightPressMs);

  if (!displayEnabled) {
    if (leftPressed || selectPressed || rightPressed) {
      displayEnabled = true;
      oled.setPowerSave(0);
      savePetState();
      Serial.println("BTN -> display on");
    }
    return;
  }

  if (petMenuOpen) {
    if (leftPressed) {
      petMenuIndex = petMenuIndex == 0 ? PET_ACTION_COUNT - 1 : petMenuIndex - 1;
    }

    if (rightPressed) {
      petMenuIndex = (petMenuIndex + 1) % PET_ACTION_COUNT;
    }

    if (selectPressed) {
      executePetAction();
    }

    return;
  }

  if (selectPressed) {
    if (currentPage == PAGE_DASHBOARD) {
      petMenuOpen = true;
      petMenuIndex = PET_ACTION_FEED;
      Serial.println("BTN SELECT -> pet menu");
    } else {
      currentPage = PAGE_DASHBOARD;
      Serial.println("BTN SELECT -> home");
    }
  }

  if (leftPressed) {
    nextPage();
    petMenuOpen = false;
    Serial.print("BTN NEXT -> page ");
    Serial.println(static_cast<uint8_t>(currentPage) + 1);
  }

  if (rightPressed) {
    previousPage();
    petMenuOpen = false;
    Serial.print("BTN PREV -> page ");
    Serial.println(static_cast<uint8_t>(currentPage) + 1);
  }
}

void drawDottedFrame(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
  for (uint8_t px = x; px < x + width; px += 2) {
    oled.drawPixel(px, y);
    oled.drawPixel(px, y + height - 1);
  }

  for (uint8_t py = y; py < y + height; py += 2) {
    oled.drawPixel(x, py);
    oled.drawPixel(x + width - 1, py);
  }
}

void drawMetricCell(uint8_t x, uint8_t y, uint8_t width, float percent, const char *label) {
  oled.setFont(u8g2_font_5x8_tf);
  const int labelWidth = oled.getStrWidth(label);
  oled.setCursor(x + (width - labelWidth) / 2, y + 9);
  oled.print(label);

  const String value = formatPercentUnit(percent);
  oled.setFont(u8g2_font_6x12_tf);
  const int valueWidth = oled.getStrWidth(value.c_str());
  oled.setCursor(x + (width - valueWidth) / 2, y + 24);
  oled.print(value);
}

void drawDateTimeBlock(uint8_t x, uint8_t y, const String &time, const String &date) {
  oled.setFont(u8g2_font_7x14B_tf);
  oled.setCursor(x, y + 13);
  oled.print(time);

  oled.setFont(u8g2_font_6x12_tf);
  oled.setCursor(x, y + 28);
  oled.print(date);
}

void drawRightAlignedText(uint8_t rightX, uint8_t baseline, const String &value) {
  oled.setFont(u8g2_font_6x12_tf);
  const int width = oled.getStrWidth(value.c_str());
  oled.setCursor(rightX - width + 1, baseline);
  oled.print(value);
}

void drawRightAlignedTemperature(uint8_t rightX, uint8_t baseline, float temperature) {
  oled.setFont(u8g2_font_6x12_tf);

  if (isnan(temperature)) {
    drawRightAlignedText(rightX, baseline, "--.- C");
    oled.drawCircle(rightX - 6, baseline - 9, 1);
    return;
  }

  const String value = String(temperature, 1) + " C";
  const int width = oled.getStrWidth(value.c_str());
  const int startX = rightX - width + 1;
  oled.setCursor(startX, baseline);
  oled.print(value);
  oled.drawCircle(rightX - 6, baseline - 9, 1);
}

void drawPageIndicator(uint8_t pageIndex) {
  oled.setFont(u8g2_font_4x6_tf);
  const String indicator = String(pageIndex + 1) + "/" + String(pageCount());
  const int indicatorWidth = oled.getStrWidth(indicator.c_str());
  oled.setCursor(128 - indicatorWidth, 6);
  oled.print(indicator);
}

void renderLoading(const String &line1, const String &line2) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.setCursor(0, 16);
  oled.print(FIRMWARE_VERSION);
  oled.setCursor(0, 34);
  oled.print(line1);
  oled.setCursor(0, 52);
  oled.print(line2);
  oled.sendBuffer();
}

bool readXbmPixel(const uint8_t *bitmap, uint8_t width, uint8_t x, uint8_t y) {
  const uint8_t bytesPerRow = (width + 7) / 8;
  const uint16_t byteIndex = static_cast<uint16_t>(y) * bytesPerRow + (x / 8);
  const uint8_t value = pgm_read_byte(bitmap + byteIndex);
  return (value & (1 << (x % 8))) != 0;
}

void drawScaledXbm(uint8_t x,
                   uint8_t y,
                   uint8_t width,
                   uint8_t height,
                   const uint8_t *bitmap,
                   uint8_t scaleNum,
                   uint8_t scaleDen,
                   bool flipX) {
  const uint8_t scaledWidth = (width * scaleNum) / scaleDen;
  const uint8_t scaledHeight = (height * scaleNum) / scaleDen;

  for (uint8_t dy = 0; dy < scaledHeight; dy++) {
    const uint8_t sourceY = (dy * scaleDen) / scaleNum;
    for (uint8_t dx = 0; dx < scaledWidth; dx++) {
      const uint8_t rawSourceX = (dx * scaleDen) / scaleNum;
      const uint8_t sourceX = flipX ? width - 1 - rawSourceX : rawSourceX;
      if (readXbmPixel(bitmap, width, sourceX, sourceY)) {
        oled.drawPixel(x + dx, y + dy);
      }
    }
  }
}

void drawTinyBar(uint8_t x, uint8_t y, uint8_t width, uint8_t value) {
  const uint8_t fill = map(value > 100 ? 100 : value, 0, 100, 0, width - 2);
  oled.drawFrame(x, y, width, 3);
  oled.drawBox(x + 1, y + 1, fill, 1);
}

void drawPetGauge(uint8_t x, uint8_t y, char label, uint8_t value) {
  oled.setFont(u8g2_font_4x6_tf);
  oled.setCursor(x, y + 5);
  oled.print(label);
  drawTinyBar(x + 6, y + 2, 22, value);
}

void drawSpeechBubble(const String &message) {
  if (message.length() == 0) {
    return;
  }

  oled.setFont(u8g2_font_5x8_tf);
  uint8_t bubbleWidth = oled.getStrWidth(message.c_str()) + 8;
  if (bubbleWidth > 46) {
    bubbleWidth = 46;
  }
  const uint8_t x = (128 - bubbleWidth) / 2;
  oled.drawRFrame(x, 0, bubbleWidth, 13, 2);
  oled.setCursor(x + 4, 9);
  oled.print(message);
}

void drawPetMenu() {
  oled.drawBox(0, 45, 128, 19);
  oled.setDrawColor(0);
  oled.setFont(u8g2_font_6x12_tf);
  oled.setCursor(4, 59);
  oled.print("<");

  const char *label = petMenuLabel(petMenuIndex);
  const int labelWidth = oled.getStrWidth(label);
  oled.setCursor((128 - labelWidth) / 2, 59);
  oled.print(label);

  oled.setCursor(119, 59);
  oled.print(">");
  oled.setDrawColor(1);
}

String petMessage() {
  const PetNoticeType notice = activePetNotice();
  if (notice == PET_NOTICE_NETWORK) {
    return "NET KO";
  }

  if (notice == PET_NOTICE_HOT) {
    return "CHAUD";
  }

  switch (currentPetBaseMode()) {
  case PET_BASE_SLEEP:
    return "ZZZ";
  case PET_BASE_HUNGRY:
    return "MIAM?";
  case PET_BASE_CHILL:
    return "CHILL";
  case PET_BASE_WORK:
    return "TRAVAIL";
  default:
    return "";
  }
}

uint8_t interpolatePetX(uint8_t fromX, uint8_t toX, uint16_t elapsedMs) {
  const int16_t distance = static_cast<int16_t>(toX) - static_cast<int16_t>(fromX);
  return fromX + (distance * elapsedMs) / PET_MOVE_MS;
}

PetRenderSprite currentPetSprite() {
  const uint32_t now = millis();
  const uint8_t frame = (now / PET_ANIMATION_FRAME_MS) % TAMAGOTCHI_BAT_FRAME_COUNT;
  const uint32_t cycleMs = (PET_IDLE_MS * 2UL) + (PET_MOVE_MS * 2UL);
  const uint16_t motionMs = now % cycleMs;
  const PetNoticeType notice = activePetNotice();
  const bool alertFace = notice == PET_NOTICE_HOT || currentPetBaseMode() == PET_BASE_HUNGRY;

  bool moving = false;
  PetRenderSprite sprite;
  sprite.x = PET_X_LEFT;

  if (motionMs < PET_IDLE_MS) {
    sprite.x = PET_X_LEFT;
  } else if (motionMs < PET_IDLE_MS + PET_MOVE_MS) {
    moving = true;
    sprite.x = interpolatePetX(PET_X_LEFT, PET_X_RIGHT, motionMs - PET_IDLE_MS);
  } else if (motionMs < (PET_IDLE_MS * 2UL) + PET_MOVE_MS) {
    sprite.x = PET_X_RIGHT;
  } else {
    moving = true;
    sprite.flipX = true;
    sprite.x = interpolatePetX(PET_X_RIGHT, PET_X_LEFT, motionMs - ((PET_IDLE_MS * 2UL) + PET_MOVE_MS));
  }

  if (notice == PET_NOTICE_NETWORK) {
    sprite.bitmap = tmg_bat_angry_hurt_frames[frame];
  } else if (moving && alertFace) {
    sprite.bitmap = tmg_bat_angry_move_frames[frame];
  } else if (moving) {
    sprite.bitmap = tmg_bat_normal_move_frames[frame];
  } else if (alertFace) {
    sprite.bitmap = tmg_bat_angry_idle_frames[frame];
  } else {
    sprite.bitmap = tmg_bat_normal_idle_frames[frame];
  }

  return sprite;
}

void renderDashboardPage() {
  oled.clearBuffer();

  String time = formatClockFromNtp();
  if (time.length() == 0) {
    time = weatherTime(weather.datetime);
  }
  if (time.length() == 0) {
    time = "--:--";
  }

  oled.setFont(u8g2_font_7x14B_tf);
  oled.setCursor(0, 11);
  oled.print(time);
  drawRightAlignedTemperature(127, 12, weather.temperature);

  drawSpeechBubble(petMessage());
  const PetRenderSprite petSprite = currentPetSprite();
  drawScaledXbm(petSprite.x,
                PET_Y,
                TAMAGOTCHI_BAT_FRAME_WIDTH,
                TAMAGOTCHI_BAT_FRAME_HEIGHT,
                petSprite.bitmap,
                PET_SCALE_NUM,
                PET_SCALE_DEN,
                petSprite.flipX);

  const PetBaseMode baseMode = currentPetBaseMode();
  const PetNoticeType notice = activePetNotice();
  const uint8_t hunger = baseMode == PET_BASE_HUNGRY ? 25 : 100;
  const uint8_t energy = notice != PET_NOTICE_NONE ? 45 : (baseMode == PET_BASE_CHILL ? 70 : 85);
  const uint8_t sleep = baseMode == PET_BASE_SLEEP ? 100 : 55;

  drawPetGauge(96, 26, 'H', hunger);
  drawPetGauge(96, 37, 'E', energy);
  drawPetGauge(96, 48, 'S', sleep);

  if (petMenuOpen) {
    drawPetMenu();
  }

  oled.sendBuffer();
}

void renderProxmoxPage() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);

  oled.setCursor(0, 10);
  oled.print("PVE DETAILS");
  drawPageIndicator(PAGE_PROXMOX);
  oled.setFont(u8g2_font_6x12_tf);

  oled.setCursor(0, 24);
  oled.print("CPU: ");
  oled.print(formatPercent(proxmox.cpu));
  oled.print("%");

  oled.setCursor(0, 38);
  oled.print("RAM: ");
  oled.print(formatPercent(proxmox.ram));
  oled.print("% ");
  oled.print(formatGb(proxmox.ramUsedGb));

  oled.setCursor(0, 52);
  oled.print("DSK: ");
  oled.print(formatPercent(proxmox.storage));
  oled.print("% ");
  oled.print(formatGb(proxmox.storageUsedGb));

  oled.setCursor(0, 64);
  oled.print("UP: ");
  oled.print(formatUptime(proxmox.uptime));

  oled.sendBuffer();
}

String otaDisplayStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    return "OFF WIFI";
  }

  if (!otaConfigured) {
    return "OFF INIT";
  }

  if (otaUiStatus == "ready" || otaUiStatus == "done") {
    return "ON :3232";
  }

  if (otaUiStatus == "start") {
    return "ON START";
  }

  if (otaUiStatus.endsWith("%")) {
    return "ON " + otaUiStatus;
  }

  String status = otaUiStatus;
  status.toUpperCase();
  return status;
}

void renderNetworkPage() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);

  oled.setCursor(0, 10);
  oled.print("NETWORK");
  drawPageIndicator(PAGE_NETWORK);
  oled.setFont(u8g2_font_6x12_tf);

  oled.setCursor(0, 24);
  oled.print("WiFi: ");
  oled.print(WiFi.RSSI());
  oled.print("dBm");

  oled.setCursor(0, 38);
  oled.print("IP: ");
  oled.print(WiFi.localIP().toString());

  oled.setCursor(0, 52);
  oled.print("PVE: ");
  oled.print(formatAge(proxmox.lastOkMs, proxmox.valid));
  oled.print("  WX: ");
  oled.print(formatAge(weather.lastOkMs, weather.valid));

  oled.setCursor(0, 64);
  oled.print("OTA: ");
  oled.print(otaDisplayStatus());

  oled.sendBuffer();
}

void renderGuestPage(uint8_t guestIndex) {
  if (guestIndex >= proxmox.guestCount) {
    renderLoading("Guest", "not found");
    return;
  }

  const GuestMetrics &guest = proxmox.guests[guestIndex];
  String type = guest.type;
  type.toUpperCase();

  String title = type + " " + String(guest.vmid) + " " + guest.name;
  if (guest.status != "running") {
    title += " " + guest.status;
  }

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);

  oled.setCursor(0, 10);
  oled.print(truncateText(title, 15));
  drawPageIndicator(BASE_PAGE_COUNT + guestIndex);
  oled.setFont(u8g2_font_6x12_tf);

  oled.setCursor(0, 24);
  oled.print("CPU: ");
  oled.print(formatPercent(guest.cpu));
  oled.print("%");

  oled.setCursor(0, 38);
  oled.print("RAM: ");
  oled.print(formatPercent(guest.ram));
  oled.print("% ");
  oled.print(formatGb(guest.ramUsedGb));

  oled.setCursor(0, 52);
  oled.print("DSK: ");
  oled.print(formatPercent(guest.storage));
  oled.print("% ");
  oled.print(formatGb(guest.storageUsedGb));

  oled.setCursor(0, 64);
  oled.print("UP: ");
  oled.print(formatUptime(guest.uptime));

  oled.sendBuffer();
}

void renderOled() {
  if (!displayEnabled) {
    return;
  }

  if (currentPage >= pageCount()) {
    currentPage = PAGE_DASHBOARD;
  }

  if (currentPage == PAGE_DASHBOARD) {
    renderDashboardPage();
    return;
  }

  if (currentPage == PAGE_NETWORK) {
    renderNetworkPage();
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    renderLoading("WiFi connect", WIFI_SSID);
    return;
  }

  if (!proxmox.valid) {
    renderLoading("PVE bridge", proxmox.error);
    return;
  }

  if (currentPage == PAGE_PROXMOX) {
    renderProxmoxPage();
    return;
  }

  if (currentPage >= BASE_PAGE_COUNT) {
    renderGuestPage(currentPage - BASE_PAGE_COUNT);
    return;
  }

  renderDashboardPage();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.print("Boot ");
  Serial.println(FIRMWARE_VERSION);

  pinMode(USER_POT_PIN, INPUT);
  pinMode(BUTTON_LEFT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT_PIN, INPUT_PULLUP);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  petPrefs.begin("pet", false);
  loadPetState();

  attachInterrupt(digitalPinToInterrupt(BUTTON_LEFT_PIN), onLeftButtonFall, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_SELECT_PIN), onSelectButtonFall, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_RIGHT_PIN), onRightButtonFall, FALLING);

#ifdef OLED_DIAGNOSTIC_MODE
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  oled.begin();
  oled.setBusClock(400000);
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.setCursor(0, 14);
  oled.print("OLED TEST");
  oled.setCursor(0, 32);
  oled.print("SDA22 SCK23");
  oled.setCursor(0, 50);
  oled.print("SH1106 SW I2C");
  oled.sendBuffer();
  Serial.println("OLED diagnostic mode");
  Serial.println("Expected OLED: OLED TEST / SDA22 SCK23 / SH1106 SW I2C");
  return;
#endif

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  oled.begin();
  oled.setBusClock(400000);
  oled.setPowerSave(displayEnabled ? 0 : 1);

  WiFi.setAutoReconnect(true);
  connectWifi();
  lastWeatherPollMs = millis();

  renderOled();
}

void loop() {
#ifdef OLED_DIAGNOSTIC_MODE
  return;
#endif

  handleButtons();
  connectWifi();
  configureTimeIfNeeded();
  configureOtaIfNeeded();
  ArduinoOTA.handle();

  const uint32_t now = millis();

  if (lastProxmoxPollMs == 0 || now - lastProxmoxPollMs >= PROXMOX_REFRESH_MS) {
    lastProxmoxPollMs = now;
    pollProxmox();
  }

  if (lastWeatherPollMs == 0 || now - lastWeatherPollMs >= WEATHER_REFRESH_MS) {
    lastWeatherPollMs = now;
    pollWeather();
  }

  updatePetRoutine();
  updateAlertLed();
  handleButtons();
  renderOled();
  delay(PET_RENDER_DELAY_MS);
}
