#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <esp_system.h>
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

#ifndef PET_INITIAL_FOOD
#define PET_INITIAL_FOOD 1
#endif

#ifndef PET_INITIAL_HUNGER
#define PET_INITIAL_HUNGER 50
#endif

#ifndef PET_INITIAL_ENERGY
#define PET_INITIAL_ENERGY 50
#endif

#ifndef PET_MAX_FOOD
#define PET_MAX_FOOD 10
#endif

#ifndef PET_HUNT_MIN_ENERGY
#define PET_HUNT_MIN_ENERGY 10
#endif

#ifndef PET_HUNT_ENERGY_COST
#define PET_HUNT_ENERGY_COST 10
#endif

#ifndef PET_HUNT_MAX_HUNGER_COST
#define PET_HUNT_MAX_HUNGER_COST 10
#endif

#ifndef PET_EAT_HUNGER_GAIN
#define PET_EAT_HUNGER_GAIN 20
#endif

#ifndef PET_EAT_MAX_ENERGY_COST
#define PET_EAT_MAX_ENERGY_COST 5
#endif

#ifndef PET_IDLE_HUNGER_LOSS_PER_HOUR
#define PET_IDLE_HUNGER_LOSS_PER_HOUR 2
#endif

#ifndef PET_SLEEP_ENERGY_GAIN_PER_HOUR
#define PET_SLEEP_ENERGY_GAIN_PER_HOUR 10
#endif

#ifndef PET_SLEEP_HUNGER_LOSS_PER_HOUR
#define PET_SLEEP_HUNGER_LOSS_PER_HOUR 3
#endif

#ifndef PET_HUNT_DURATION_MS
#define PET_HUNT_DURATION_MS (60UL * 60UL * 1000UL)
#endif

#ifndef PET_EAT_DURATION_MS
#define PET_EAT_DURATION_MS (10UL * 60UL * 1000UL)
#endif

#ifndef PET_STAT_TICK_MS
#define PET_STAT_TICK_MS (60UL * 60UL * 1000UL)
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
  PET_ACTION_HUNT = 0,
  PET_ACTION_EAT = 1,
  PET_ACTION_SLEEP = 2,
  PET_ACTION_SCREEN_OFF = 3,
  PET_ACTION_BACK = 4,
  PET_ACTION_COUNT = 5,
};

enum PetActionState : uint8_t {
  PET_STATE_IDLE = 0,
  PET_STATE_HUNT = 1,
  PET_STATE_EAT = 2,
  PET_STATE_SLEEP = 3,
};

enum PetNoticeType : uint8_t {
  PET_NOTICE_NONE = 0,
  PET_NOTICE_HOT = 1,
  PET_NOTICE_NETWORK = 2,
  PET_NOTICE_NO_ENERGY = 3,
  PET_NOTICE_NO_FOOD = 4,
  PET_NOTICE_NO_HUNGRY = 5,
  PET_NOTICE_BUSY = 6,
};

struct PetRenderSprite {
  const uint8_t *bitmap = nullptr;
  uint8_t x = 0;
  bool flipX = false;
};

struct PetState {
  uint8_t hunger = PET_INITIAL_HUNGER;
  uint8_t energy = PET_INITIAL_ENERGY;
  uint8_t food = PET_INITIAL_FOOD;
  PetActionState action = PET_STATE_IDLE;
  uint32_t actionStartedMs = 0;
  uint32_t statTickMs = 0;
  int32_t sleepWakeDay = -1;
  bool hotLatched = false;
  bool connectionAlertLatched = false;
  PetNoticeType notice = PET_NOTICE_NONE;
  uint32_t noticeUntilMs = 0;
};

uint8_t currentPage = PAGE_DASHBOARD;
bool displayEnabled = true;
bool petMenuOpen = false;
uint8_t petMenuIndex = PET_ACTION_HUNT;
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
const uint8_t PET_Y = 10;
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
  pet.hunger = petPrefs.getUChar("hunger", PET_INITIAL_HUNGER);
  pet.energy = petPrefs.getUChar("energy", PET_INITIAL_ENERGY);
  pet.food = petPrefs.getUChar("food", PET_INITIAL_FOOD);
  pet.action = static_cast<PetActionState>(petPrefs.getUChar("action", PET_STATE_IDLE));
  pet.sleepWakeDay = petPrefs.getInt("wakeDay", -1);
  pet.actionStartedMs = millis();
  pet.statTickMs = millis();

  if (pet.hunger > 100) {
    pet.hunger = PET_INITIAL_HUNGER;
  }
  if (pet.energy > 100) {
    pet.energy = PET_INITIAL_ENERGY;
  }
  if (pet.food > PET_MAX_FOOD) {
    pet.food = PET_MAX_FOOD;
  }
  if (pet.action != PET_STATE_SLEEP) {
    pet.action = PET_STATE_IDLE;
  }

  displayEnabled = petPrefs.getBool("display", true);
}

void savePetState() {
  petPrefs.putUChar("hunger", pet.hunger);
  petPrefs.putUChar("energy", pet.energy);
  petPrefs.putUChar("food", pet.food);
  petPrefs.putUChar("action", static_cast<uint8_t>(pet.action));
  petPrefs.putInt("wakeDay", pet.sleepWakeDay);
  petPrefs.putBool("display", displayEnabled);
}

bool hasConnectionAlert() {
  return WiFi.status() != WL_CONNECTED || !proxmox.valid;
}

uint8_t clampPetStat(int16_t value) {
  if (value <= 0) {
    return 0;
  }
  if (value >= 100) {
    return 100;
  }
  return static_cast<uint8_t>(value);
}

uint8_t clampFood(int16_t value) {
  if (value <= 0) {
    return 0;
  }
  if (value >= PET_MAX_FOOD) {
    return PET_MAX_FOOD;
  }
  return static_cast<uint8_t>(value);
}

bool isPetBusy() {
  return pet.action == PET_STATE_HUNT || pet.action == PET_STATE_EAT;
}

bool isPetSleeping() {
  return pet.action == PET_STATE_SLEEP;
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

int32_t nextWakeDay(const struct tm &timeInfo) {
  const int32_t today = dayKey(timeInfo);
  const uint16_t nowMinutes = minutesSinceMidnight(timeInfo);
  const uint16_t wakeMinutes = configuredMinutes(PET_WAKE_HOUR, PET_WAKE_MINUTE);
  return nowMinutes < wakeMinutes ? today : today + 1;
}

uint8_t rollHuntFood() {
  const long roll = random(100);
  if (roll < 50) {
    return 0;
  }
  if (roll < 80) {
    return 1;
  }
  if (roll < 95) {
    return 2;
  }
  return 4;
}

void finishPetAction() {
  switch (pet.action) {
  case PET_STATE_HUNT: {
    const uint8_t foundFood = rollHuntFood();
    const uint8_t hungerCost = random(PET_HUNT_MAX_HUNGER_COST + 1);
    pet.energy = clampPetStat(static_cast<int16_t>(pet.energy) - PET_HUNT_ENERGY_COST);
    pet.hunger = clampPetStat(static_cast<int16_t>(pet.hunger) - hungerCost);
    pet.food = clampFood(static_cast<int16_t>(pet.food) + foundFood);
    pet.action = PET_STATE_IDLE;
    break;
  }
  case PET_STATE_EAT: {
    const uint8_t energyCost = random(PET_EAT_MAX_ENERGY_COST + 1);
    pet.food = clampFood(static_cast<int16_t>(pet.food) - 1);
    pet.hunger = clampPetStat(static_cast<int16_t>(pet.hunger) + PET_EAT_HUNGER_GAIN);
    pet.energy = clampPetStat(static_cast<int16_t>(pet.energy) - energyCost);
    pet.action = PET_STATE_IDLE;
    break;
  }
  default:
    break;
  }

  pet.actionStartedMs = millis();
  pet.statTickMs = millis();
  savePetState();
}

void wakePet() {
  pet.action = PET_STATE_IDLE;
  pet.sleepWakeDay = -1;
  pet.actionStartedMs = millis();
  pet.statTickMs = millis();
  savePetState();
}

bool startPetAction(PetActionState action) {
  if (isPetBusy() || (isPetSleeping() && action != PET_STATE_IDLE)) {
    startPetNotice(PET_NOTICE_BUSY);
    return false;
  }

  switch (action) {
  case PET_STATE_HUNT:
    if (pet.energy < PET_HUNT_MIN_ENERGY) {
      startPetNotice(PET_NOTICE_NO_ENERGY);
      return false;
    }
    break;
  case PET_STATE_EAT:
    if (pet.food == 0) {
      startPetNotice(PET_NOTICE_NO_FOOD);
      return false;
    }
    if (pet.hunger >= 100) {
      startPetNotice(PET_NOTICE_NO_HUNGRY);
      return false;
    }
    break;
  case PET_STATE_SLEEP: {
    struct tm timeInfo;
    pet.sleepWakeDay = readLocalTime(timeInfo) ? nextWakeDay(timeInfo) : -1;
    break;
  }
  default:
    break;
  }

  pet.action = action;
  pet.actionStartedMs = millis();
  pet.statTickMs = millis();
  savePetState();
  return true;
}

void applyHourlyPetTick() {
  switch (pet.action) {
  case PET_STATE_IDLE:
    pet.hunger = clampPetStat(static_cast<int16_t>(pet.hunger) - PET_IDLE_HUNGER_LOSS_PER_HOUR);
    break;
  case PET_STATE_SLEEP:
    pet.energy = clampPetStat(static_cast<int16_t>(pet.energy) + PET_SLEEP_ENERGY_GAIN_PER_HOUR);
    pet.hunger = clampPetStat(static_cast<int16_t>(pet.hunger) - PET_SLEEP_HUNGER_LOSS_PER_HOUR);
    break;
  default:
    break;
  }
}

bool shouldAutoWake() {
  if (!isPetSleeping() || pet.sleepWakeDay < 0) {
    return false;
  }

  struct tm timeInfo;
  if (!readLocalTime(timeInfo)) {
    return false;
  }

  const uint16_t wakeMinutes = configuredMinutes(PET_WAKE_HOUR, PET_WAKE_MINUTE);
  return dayKey(timeInfo) >= pet.sleepWakeDay && minutesSinceMidnight(timeInfo) >= wakeMinutes;
}

void updatePetEngine() {
  bool changed = false;
  const uint32_t now = millis();

  if (pet.action == PET_STATE_HUNT && now - pet.actionStartedMs >= PET_HUNT_DURATION_MS) {
    finishPetAction();
    return;
  }

  if (pet.action == PET_STATE_EAT && now - pet.actionStartedMs >= PET_EAT_DURATION_MS) {
    finishPetAction();
    return;
  }

  while (now - pet.statTickMs >= PET_STAT_TICK_MS) {
    pet.statTickMs += PET_STAT_TICK_MS;
    applyHourlyPetTick();
    changed = true;
  }

  if (shouldAutoWake()) {
    wakePet();
    return;
  }

  if (changed) {
    savePetState();
  }
}

void updatePetRoutine() {

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

  updatePetEngine();
}

void updateAlertLed() {
  digitalWrite(RED_LED_PIN, activePetNotice() == PET_NOTICE_HOT ? HIGH : LOW);
}

const char *petMenuLabel(uint8_t index) {
  if (isPetSleeping()) {
    switch (index) {
    case 0:
      return "WAKE UP";
    case 1:
      return "SCREEN OFF";
    default:
      return "";
    }
  }

  switch (index) {
  case PET_ACTION_HUNT:
    return "HUNT";
  case PET_ACTION_EAT:
    return "EAT";
  case PET_ACTION_SLEEP:
    return "SLEEP";
  case PET_ACTION_SCREEN_OFF:
    return "SCREEN OFF";
  case PET_ACTION_BACK:
    return "BACK";
  default:
    return "";
  }
}

uint8_t petMenuActionCount() {
  return isPetSleeping() ? 2 : PET_ACTION_COUNT;
}

void executePetAction() {
  if (isPetSleeping()) {
    switch (petMenuIndex) {
    case 0:
      petMenuOpen = false;
      wakePet();
      break;
    case 1:
      petMenuOpen = false;
      displayEnabled = false;
      oled.setPowerSave(1);
      savePetState();
      break;
    default:
      petMenuOpen = false;
      break;
    }
    return;
  }

  switch (petMenuIndex) {
  case PET_ACTION_HUNT:
    petMenuOpen = false;
    startPetAction(PET_STATE_HUNT);
    break;
  case PET_ACTION_EAT:
    petMenuOpen = false;
    startPetAction(PET_STATE_EAT);
    break;
  case PET_ACTION_SLEEP:
    petMenuOpen = false;
    startPetAction(PET_STATE_SLEEP);
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
      const uint8_t actionCount = petMenuActionCount();
      petMenuIndex = petMenuIndex == 0 ? actionCount - 1 : petMenuIndex - 1;
    }

    if (rightPressed) {
      petMenuIndex = (petMenuIndex + 1) % petMenuActionCount();
    }

    if (selectPressed) {
      executePetAction();
    }

    return;
  }

  if (selectPressed) {
    if (currentPage == PAGE_DASHBOARD) {
      if (isPetBusy()) {
        startPetNotice(PET_NOTICE_BUSY);
        Serial.println("BTN SELECT -> pet busy");
      } else {
        petMenuOpen = true;
        petMenuIndex = 0;
        Serial.println("BTN SELECT -> pet menu");
      }
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
  oled.setFont(u8g2_font_5x8_tf);
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

void drawPixelStatLabel(uint8_t x, uint8_t y, char label) {
  const uint8_t *rows = nullptr;
  static const uint8_t eRows[] = {0b111, 0b100, 0b111, 0b100, 0b111};
  static const uint8_t hRows[] = {0b101, 0b101, 0b111, 0b101, 0b101};

  if (label == 'E') {
    rows = eRows;
  } else if (label == 'H') {
    rows = hRows;
  }

  if (rows == nullptr) {
    return;
  }

  for (uint8_t row = 0; row < 5; row++) {
    for (uint8_t col = 0; col < 3; col++) {
      if ((rows[row] & (1 << (2 - col))) != 0) {
        oled.drawPixel(x + col, y + row);
      }
    }
  }
}

void drawSegmentStatBar(uint8_t x, uint8_t y, char label, uint8_t value) {
  constexpr uint8_t labelWidth = 3;
  constexpr uint8_t labelGap = 4;
  constexpr uint8_t segmentWidth = 2;
  constexpr uint8_t segmentHeight = 5;
  constexpr uint8_t segmentGap = 1;
  constexpr uint8_t maxSegments = 10;

  drawPixelStatLabel(x, y, label);

  const uint8_t clampedValue = value > 100 ? 100 : value;
  const uint8_t filledSegments = clampedValue / 10;
  const uint8_t barX = x + labelWidth + labelGap;

  for (uint8_t segment = 0; segment < filledSegments && segment < maxSegments; segment++) {
    const uint8_t segmentX = barX + segment * (segmentWidth + segmentGap);
    oled.drawBox(segmentX, y, segmentWidth, segmentHeight);
  }
}

void drawPetGauge(uint8_t x, uint8_t y, char label, uint8_t value) {
  oled.setFont(u8g2_font_4x6_tf);
  oled.setCursor(x, y + 5);
  oled.print(label);
  drawTinyBar(x + 6, y + 2, 22, value);
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

String actionRemainingLabel() {
  uint32_t durationMs = 0;

  switch (pet.action) {
  case PET_STATE_HUNT:
    durationMs = PET_HUNT_DURATION_MS;
    break;
  case PET_STATE_EAT:
    durationMs = PET_EAT_DURATION_MS;
    break;
  default:
    return "";
  }

  const uint32_t elapsedMs = millis() - pet.actionStartedMs;
  const uint32_t remainingMs = elapsedMs >= durationMs ? 0 : durationMs - elapsedMs;
  const uint16_t remainingMinutes = (remainingMs + 59999UL) / 60000UL;

  if (remainingMinutes >= 60) {
    const uint16_t hours = remainingMinutes / 60;
    const uint16_t minutes = remainingMinutes % 60;
    if (minutes == 0) {
      return String(hours) + "H";
    }
    return String(hours) + "H" + String(minutes);
  }

  return String(remainingMinutes) + "M";
}

String petMessage() {
  const PetNoticeType notice = activePetNotice();
  if (notice == PET_NOTICE_NETWORK) {
    return "NET KO";
  }

  if (notice == PET_NOTICE_HOT) {
    return "HOT";
  }

  if (notice == PET_NOTICE_NO_ENERGY) {
    return "NO ENERGY";
  }

  if (notice == PET_NOTICE_NO_FOOD) {
    return "NO FOOD";
  }

  if (notice == PET_NOTICE_NO_HUNGRY) {
    return "NO HUNGRY";
  }

  if (notice == PET_NOTICE_BUSY) {
    return "BUSY";
  }

  switch (pet.action) {
  case PET_STATE_HUNT:
    return "HUNT " + actionRemainingLabel();
  case PET_STATE_EAT:
    return "EAT " + actionRemainingLabel();
  case PET_STATE_SLEEP:
    return "SLEEP";
  case PET_STATE_IDLE:
    if (pet.energy == 0) {
      return "NO ENERGY";
    }
    if (pet.hunger <= 20) {
      return "STARVING";
    }
    if (pet.energy <= 20) {
      return "TIRED";
    }
    return "";
  default:
    return "";
  }
}

String petActivityLabel() {
  switch (pet.action) {
  case PET_STATE_HUNT:
    return "HUNT / " + actionRemainingLabel();
  case PET_STATE_EAT:
    return "EAT / " + actionRemainingLabel();
  case PET_STATE_SLEEP:
    return "SLEEP";
  default:
    return petMessage();
  }
}

void drawRightAlignedActivity(uint8_t rightX, uint8_t baseline, const String &message) {
  if (message.length() == 0) {
    return;
  }

  oled.setFont(u8g2_font_4x6_tf);
  const int textWidth = oled.getStrWidth(message.c_str());
  oled.setCursor(rightX - textWidth + 1, baseline);
  oled.print(message);
}

uint8_t interpolatePetX(uint8_t fromX, uint8_t toX, uint16_t elapsedMs) {
  const int16_t distance = static_cast<int16_t>(toX) - static_cast<int16_t>(fromX);
  return fromX + (distance * elapsedMs) / PET_MOVE_MS;
}

PetRenderSprite currentPetSprite() {
  const uint32_t now = millis();
  const uint8_t frame = (now / PET_ANIMATION_FRAME_MS) % TAMAGOTCHI_BAT_FRAME_COUNT;

  if (pet.action == PET_STATE_SLEEP) {
    PetRenderSprite sprite;
    sprite.x = PET_X_LEFT;
    sprite.bitmap = tmg_bat_sleep_frames[frame];
    return sprite;
  }

  if (pet.action == PET_STATE_EAT) {
    PetRenderSprite sprite;
    sprite.x = PET_X_LEFT;
    sprite.bitmap = tmg_bat_eating_frames[frame];
    return sprite;
  }

  const uint32_t cycleMs = (PET_IDLE_MS * 2UL) + (PET_MOVE_MS * 2UL);
  const uint16_t motionMs = now % cycleMs;
  const PetNoticeType notice = activePetNotice();
  const bool hurtFace = notice != PET_NOTICE_NONE || pet.hunger <= 20 || pet.energy <= 20;

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

  if (pet.action == PET_STATE_HUNT) {
    sprite.bitmap = moving ? tmg_bat_angry_move_frames[frame] : tmg_bat_angry_idle_frames[frame];
  } else if (hurtFace) {
    sprite.bitmap = tmg_bat_normal_hurt_frames[frame];
  } else if (moving) {
    sprite.bitmap = tmg_bat_normal_move_frames[frame];
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

  const PetRenderSprite petSprite = currentPetSprite();
  drawScaledXbm(petSprite.x,
                PET_Y,
                TAMAGOTCHI_BAT_FRAME_WIDTH,
                TAMAGOTCHI_BAT_FRAME_HEIGHT,
                petSprite.bitmap,
                PET_SCALE_NUM,
                PET_SCALE_DEN,
                petSprite.flipX);

  drawRightAlignedActivity(127, 47, petActivityLabel());
  drawSegmentStatBar(91, 52, 'E', pet.energy);
  drawSegmentStatBar(91, 59, 'H', pet.hunger);

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
  randomSeed(esp_random());

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
