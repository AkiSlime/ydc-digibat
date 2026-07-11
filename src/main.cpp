#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
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
#define PET_HUNT_MIN_ENERGY 2
#endif

#ifndef PET_HUNT_ENERGY_COST
#define PET_HUNT_ENERGY_COST 2
#endif

#ifndef PET_HUNT_MAX_HUNGER_COST
#define PET_HUNT_MAX_HUNGER_COST 2
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

#ifndef PET_SLEEP_ENERGY_GAIN
#ifdef PET_SLEEP_ENERGY_GAIN_PER_HOUR
#define PET_SLEEP_ENERGY_GAIN PET_SLEEP_ENERGY_GAIN_PER_HOUR
#else
#define PET_SLEEP_ENERGY_GAIN 10
#endif
#endif

#ifndef PET_SLEEP_ENERGY_TICK_MS
#define PET_SLEEP_ENERGY_TICK_MS (30UL * 60UL * 1000UL)
#endif

#ifndef PET_SLEEP_HUNGER_LOSS_PER_HOUR
#define PET_SLEEP_HUNGER_LOSS_PER_HOUR 3
#endif

#ifndef PET_HUNT_DURATION_MS
#define PET_HUNT_DURATION_MS (10UL * 60UL * 1000UL)
#endif

#ifndef PET_EAT_DURATION_MS
#define PET_EAT_DURATION_MS (3UL * 60UL * 1000UL)
#endif

#ifndef PET_STAT_TICK_MS
#define PET_STAT_TICK_MS (60UL * 60UL * 1000UL)
#endif

#ifndef PET_INITIAL_LEVEL
#define PET_INITIAL_LEVEL 1
#endif

#ifndef PET_INITIAL_XP
#define PET_INITIAL_XP 0
#endif

#ifndef PET_INITIAL_HP
#define PET_INITIAL_HP 10
#endif

#ifndef PET_INITIAL_ATK
#define PET_INITIAL_ATK 2
#endif

#ifndef PET_INITIAL_DEF
#define PET_INITIAL_DEF 1
#endif

#ifndef PET_INITIAL_LCK
#define PET_INITIAL_LCK 1
#endif

#ifndef PET_MAX_LEVEL
#define PET_MAX_LEVEL 99
#endif

#ifndef PET_ARENA_MIN_ENERGY
#define PET_ARENA_MIN_ENERGY 5
#endif

#ifndef PET_ARENA_ENERGY_COST
#define PET_ARENA_ENERGY_COST 5
#endif

#ifndef PET_ARENA_HUNGER_COST
#define PET_ARENA_HUNGER_COST 5
#endif

#ifndef PET_ARENA_COMBAT_INTERVAL_MS
#define PET_ARENA_COMBAT_INTERVAL_MS (2UL * 60UL * 1000UL)
#endif

#ifndef PET_ARENA_XP_PERCENT_AT_TARGET
#define PET_ARENA_XP_PERCENT_AT_TARGET 35
#endif

#ifndef PROXMOX_METRICS_URL
#define PROXMOX_METRICS_URL ""
#endif

#ifndef WEATHER_URL
#define WEATHER_URL ""
#endif

#ifndef DASHBOARD_WEATHER_ENABLED_DEFAULT
#define DASHBOARD_WEATHER_ENABLED_DEFAULT 0
#endif

#ifndef DASHBOARD_PROXMOX_ENABLED_DEFAULT
#define DASHBOARD_PROXMOX_ENABLED_DEFAULT 0
#endif

#ifndef DASHBOARD_HOME_INFO_MODE_DEFAULT
#define DASHBOARD_HOME_INFO_MODE_DEFAULT 0
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
ProxmoxMetrics pendingProxmox;
WeatherMetrics pendingWeather;
Preferences petPrefs;

const char *FIRMWARE_VERSION = "OLED v15 PET";

enum DashboardPage : uint8_t {
  PAGE_DASHBOARD = 0,
  PAGE_PROXMOX = 1,
  PAGE_NETWORK = 2,
};

enum PetMenuAction : uint8_t {
  PET_ACTION_SHEET = 0,
  PET_ACTION_SLEEP = 1,
  PET_ACTION_EAT = 2,
  PET_ACTION_HUNT = 3,
  PET_ACTION_ARENA = 4,
  PET_ACTION_SETTINGS = 5,
  PET_ACTION_SCREEN_OFF = 6,
  PET_ACTION_RESET = 7,
  PET_ACTION_BACK = 8,
  PET_ACTION_COUNT = 9,
};

enum PetMenuMode : uint8_t {
  PET_MENU_MAIN = 0,
  PET_MENU_HUNT_RUNS = 1,
  PET_MENU_ACTIVITY = 2,
  PET_MENU_EAT_RUNS = 3,
  PET_MENU_ARENA_RUNS = 4,
  PET_MENU_RESET_CONFIRM = 5,
  PET_MENU_SETTINGS = 6,
};

enum PetActionState : uint8_t {
  PET_STATE_IDLE = 0,
  PET_STATE_HUNT = 1,
  PET_STATE_EAT = 2,
  PET_STATE_SLEEP = 3,
  PET_STATE_ARENA = 4,
};

enum PetNoticeType : uint8_t {
  PET_NOTICE_NONE = 0,
  PET_NOTICE_HOT = 1,
  PET_NOTICE_NETWORK = 2,
  PET_NOTICE_NO_ENERGY = 3,
  PET_NOTICE_NO_FOOD = 4,
  PET_NOTICE_FULL = 5,
  PET_NOTICE_BUSY = 6,
};

enum PetResultType : uint8_t {
  PET_RESULT_NONE = 0,
  PET_RESULT_HUNT = 1,
  PET_RESULT_EAT = 2,
};

enum HomeInfoMode : uint8_t {
  HOME_INFO_AUTO = 0,
  HOME_INFO_TEMP = 1,
  HOME_INFO_CLOCK = 2,
};

bool startPetAction(PetActionState action);

struct PetRenderSprite {
  const uint8_t *bitmap = nullptr;
  uint8_t x = 0;
  bool flipX = false;
};

struct PetState {
  uint8_t hunger = PET_INITIAL_HUNGER;
  uint8_t energy = PET_INITIAL_ENERGY;
  uint8_t food = PET_INITIAL_FOOD;
  uint8_t level = PET_INITIAL_LEVEL;
  uint16_t xp = PET_INITIAL_XP;
  uint8_t hp = PET_INITIAL_HP;
  uint8_t atk = PET_INITIAL_ATK;
  uint8_t def = PET_INITIAL_DEF;
  uint8_t lck = PET_INITIAL_LCK;
  uint16_t arenaBest = 0;
  uint16_t arenaLast = 0;
  uint16_t arenaWins = 0;
  int16_t arenaRunHp = PET_INITIAL_HP;
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
bool petSheetOpen = false;
bool arenaResultOpen = false;
bool petResultOpen = false;
PetResultType petResultType = PET_RESULT_NONE;
PetMenuMode petMenuMode = PET_MENU_MAIN;
uint8_t petMenuIndex = PET_ACTION_SHEET;
uint8_t petSheetPage = 0;
uint16_t arenaResultWins = 0;
uint16_t arenaResultXp = 0;
uint8_t arenaResultLevel = PET_INITIAL_LEVEL;
uint8_t arenaResultRuns = 0;
int16_t petResultFoodDelta = 0;
int16_t petResultHungerDelta = 0;
int16_t petResultEnergyDelta = 0;
uint8_t petResultRuns = 0;
uint8_t huntRunsRemaining = 0;
uint8_t eatRunsRemaining = 0;
uint8_t arenaRunsRemaining = 0;
uint32_t arenaAnimationStepId = 0;
uint8_t arenaAnimationChoice = 0;
bool arenaAnimationChoiceReady = false;
volatile bool weatherEnabled = DASHBOARD_WEATHER_ENABLED_DEFAULT != 0;
volatile bool proxmoxEnabled = DASHBOARD_PROXMOX_ENABLED_DEFAULT != 0;
HomeInfoMode homeInfoMode = HOME_INFO_AUTO;
bool timeConfigured = false;
bool otaConfigured = false;
String otaUiStatus = "init";
uint8_t otaUiProgress = 0;

uint32_t lastWifiAttemptMs = 0;
uint32_t weatherNoticeUntilMs = 0;
uint32_t lastNextPressMs = 0;
uint32_t lastSelectPressMs = 0;
uint32_t lastPreviousPressMs = 0;
bool taskLedPatternActive = false;
uint8_t taskLedPatternStep = 0;
uint32_t taskLedPatternStepStartedMs = 0;

volatile bool nextButtonPending = false;
volatile bool selectButtonPending = false;
volatile bool previousButtonPending = false;
volatile bool proxmoxPollRequested = false;
volatile bool weatherPollRequested = false;

TaskHandle_t networkTaskHandle = nullptr;
SemaphoreHandle_t networkResultMutex = nullptr;
bool pendingProxmoxReady = false;
bool pendingWeatherReady = false;

const uint32_t BUTTON_LOCKOUT_MS = 300;
const uint32_t WEATHER_NOTICE_MS = 5000;
const uint32_t NETWORK_TASK_IDLE_MS = 100;
const uint32_t NETWORK_WIFI_RETRY_MS = 1000;
const uint32_t NETWORK_ERROR_BACKOFF_MS = 30000;
const uint32_t NETWORK_TASK_STACK_SIZE = 8192;
const uint8_t TASK_LED_PATTERN_STEP_COUNT = 5;
const uint16_t TASK_LED_FAST_MS = 500;
const uint16_t TASK_LED_SLOW_MS = 1000;
const uint16_t TASK_LED_PATTERN_MS[TASK_LED_PATTERN_STEP_COUNT] = {
    TASK_LED_FAST_MS,
    TASK_LED_FAST_MS,
    TASK_LED_SLOW_MS,
    TASK_LED_FAST_MS,
    TASK_LED_FAST_MS};
const bool TASK_LED_PATTERN_ON[TASK_LED_PATTERN_STEP_COUNT] = {true, false, true, false, true};
const uint16_t PET_ANIMATION_FRAME_MS = 180;
const uint16_t PET_ARENA_ATTACK_PAUSE_MS = 260;
const uint8_t PET_ARENA_RANDOM_ANIMATION_COUNT = TAMAGOTCHI_BAT_ATTACK_COUNT + 1;
const uint8_t PET_SHEET_PAGE_COUNT = 4;
const uint16_t PET_IDLE_MS = 3500;
const uint16_t PET_MOVE_MS = 1800;
const uint8_t PET_RENDER_DELAY_MS = 20;
const uint8_t PET_SCALE_NUM = 2;
const uint8_t PET_SCALE_DEN = 1;
const uint8_t PET_X_LEFT = 0;
const uint8_t PET_X_RIGHT = 24;
const uint8_t PET_Y = 10;
PetState pet;

void IRAM_ATTR onNextButtonFall() {
  nextButtonPending = true;
}

void IRAM_ATTR onSelectButtonFall() {
  selectButtonPending = true;
}

void IRAM_ATTR onPreviousButtonFall() {
  previousButtonPending = true;
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

bool fetchProxmoxMetrics(ProxmoxMetrics &result) {
  result = ProxmoxMetrics();
  JsonDocument doc;
  String error;

  if (!getJson(PROXMOX_METRICS_URL, doc, error)) {
    result.error = error;
    return false;
  }

  result.cpu = doc["cpu"] | NAN;
  result.ram = doc["ram"] | NAN;
  result.ramUsedGb = doc["ram_used_gb"] | NAN;
  result.ramTotalGb = doc["ram_total_gb"] | NAN;
  result.storage = doc["storage"] | NAN;
  result.storageUsedGb = doc["storage_used_gb"] | NAN;
  result.storageTotalGb = doc["storage_total_gb"] | NAN;
  result.uptime = doc["uptime"] | 0;

  JsonArray guests = doc["guests"].as<JsonArray>();
  if (!guests.isNull()) {
    for (JsonObject guest : guests) {
      if (result.guestCount >= MAX_GUESTS) {
        break;
      }

      GuestMetrics &target = result.guests[result.guestCount++];
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

  if (isnan(result.cpu) || isnan(result.ram) || isnan(result.storage)) {
    result.error = "fields";
    return false;
  }

  result.valid = true;
  result.error = "";
  result.lastOkMs = millis();
  return true;
}

bool fetchWeatherMetrics(WeatherMetrics &result) {
  result = WeatherMetrics();
  JsonDocument doc;
  String error;

  if (!getJson(WEATHER_URL, doc, error)) {
    result.error = error;
    return false;
  }

  result.temperature = doc["temperature"] | NAN;
  result.humidity = doc["humidity"] | NAN;
  result.status = doc["status"] | "";
  result.datetime = doc["datetime"] | "";
  result.ip = doc["ip"] | "";

  if (isnan(result.temperature) || isnan(result.humidity)) {
    result.error = "fields";
    return false;
  }

  result.valid = true;
  result.error = "";
  result.lastOkMs = millis();
  return true;
}

void queueProxmoxResult(const ProxmoxMetrics &result) {
  if (networkResultMutex == nullptr) {
    return;
  }

  if (xSemaphoreTake(networkResultMutex, portMAX_DELAY) == pdTRUE) {
    pendingProxmox = result;
    pendingProxmoxReady = true;
    xSemaphoreGive(networkResultMutex);
  }
}

void queueWeatherResult(const WeatherMetrics &result) {
  if (networkResultMutex == nullptr) {
    return;
  }

  if (xSemaphoreTake(networkResultMutex, portMAX_DELAY) == pdTRUE) {
    pendingWeather = result;
    pendingWeatherReady = true;
    xSemaphoreGive(networkResultMutex);
  }
}

void applyPendingNetworkResults() {
  if (networkResultMutex == nullptr) {
    return;
  }

  ProxmoxMetrics nextProxmox;
  WeatherMetrics nextWeather;
  bool hasProxmoxResult = false;
  bool hasWeatherResult = false;

  if (xSemaphoreTake(networkResultMutex, 0) == pdTRUE) {
    if (pendingProxmoxReady) {
      nextProxmox = pendingProxmox;
      pendingProxmoxReady = false;
      hasProxmoxResult = true;
    }
    if (pendingWeatherReady) {
      nextWeather = pendingWeather;
      pendingWeatherReady = false;
      hasWeatherResult = true;
    }
    xSemaphoreGive(networkResultMutex);
  }

  if (hasProxmoxResult && proxmoxEnabled) {
    if (!nextProxmox.valid) {
      nextProxmox.lastOkMs = proxmox.lastOkMs;
    }
    proxmox = nextProxmox;
  }

  if (hasWeatherResult && weatherEnabled) {
    const bool wasValid = weather.valid;
    if (!nextWeather.valid) {
      nextWeather.lastOkMs = weather.lastOkMs;
    }
    weather = nextWeather;

    if (weather.valid) {
      weatherNoticeUntilMs = 0;
    } else if (wasValid) {
      weatherNoticeUntilMs = millis() + WEATHER_NOTICE_MS;
      Serial.print("Weather disconnected: ");
      Serial.println(weather.error);
    }
  }
}

bool networkDeadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void networkTask(void *parameter) {
  (void)parameter;
  uint32_t nextProxmoxPollMs = 0;
  uint32_t nextWeatherPollMs = millis() + WEATHER_REFRESH_MS;

  for (;;) {
    const uint32_t now = millis();

    if (proxmoxEnabled &&
        (proxmoxPollRequested || networkDeadlineReached(now, nextProxmoxPollMs))) {
      proxmoxPollRequested = false;
      ProxmoxMetrics result;
      const bool valid = fetchProxmoxMetrics(result);
      queueProxmoxResult(result);
      const uint32_t retryMs = valid ? PROXMOX_REFRESH_MS
                                     : (result.error == "wifi" ? NETWORK_WIFI_RETRY_MS
                                                               : NETWORK_ERROR_BACKOFF_MS);
      nextProxmoxPollMs = millis() + retryMs;
    }

    if (weatherEnabled &&
        (weatherPollRequested || networkDeadlineReached(now, nextWeatherPollMs))) {
      weatherPollRequested = false;
      WeatherMetrics result;
      const bool valid = fetchWeatherMetrics(result);
      queueWeatherResult(result);
      const uint32_t retryMs = valid ? WEATHER_REFRESH_MS
                                     : (result.error == "wifi" ? NETWORK_WIFI_RETRY_MS
                                                               : NETWORK_ERROR_BACKOFF_MS);
      nextWeatherPollMs = millis() + retryMs;
    }

    vTaskDelay(pdMS_TO_TICKS(NETWORK_TASK_IDLE_MS));
  }
}

void startNetworkTask() {
  if (networkTaskHandle != nullptr) {
    return;
  }

  networkResultMutex = xSemaphoreCreateMutex();
  if (networkResultMutex == nullptr) {
    Serial.println("Network task mutex failed");
    return;
  }

  const BaseType_t taskCreated = xTaskCreatePinnedToCore(
      networkTask,
      "dashboard-network",
      NETWORK_TASK_STACK_SIZE,
      nullptr,
      1,
      &networkTaskHandle,
      0);

  if (taskCreated != pdPASS) {
    networkTaskHandle = nullptr;
    vSemaphoreDelete(networkResultMutex);
    networkResultMutex = nullptr;
    Serial.println("Network task start failed");
  }
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

void clearProxmoxMetrics(const char *error) {
  proxmox = ProxmoxMetrics();
  proxmox.error = error;
}

void clearWeatherMetrics(const char *error) {
  weather = WeatherMetrics();
  weather.error = error;
  weatherNoticeUntilMs = 0;
}

void loadPetState() {
  pet.hunger = petPrefs.getUChar("hunger", PET_INITIAL_HUNGER);
  pet.energy = petPrefs.getUChar("energy", PET_INITIAL_ENERGY);
  pet.food = petPrefs.getUChar("food", PET_INITIAL_FOOD);
  pet.level = petPrefs.getUChar("level", PET_INITIAL_LEVEL);
  pet.xp = petPrefs.getUShort("xp", PET_INITIAL_XP);
  pet.hp = petPrefs.getUChar("hp", PET_INITIAL_HP);
  pet.atk = petPrefs.getUChar("atk", PET_INITIAL_ATK);
  pet.def = petPrefs.getUChar("def", PET_INITIAL_DEF);
  pet.lck = petPrefs.getUChar("lck", PET_INITIAL_LCK);
  pet.arenaBest = petPrefs.getUShort("arenaBest", 0);
  pet.arenaLast = petPrefs.getUShort("arenaLast", 0);
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
  if (pet.level < 1 || pet.level > PET_MAX_LEVEL) {
    pet.level = PET_INITIAL_LEVEL;
  }
  if (pet.hp == 0) {
    pet.hp = PET_INITIAL_HP;
  }
  if (pet.atk == 0) {
    pet.atk = PET_INITIAL_ATK;
  }
  if (pet.def == 0) {
    pet.def = PET_INITIAL_DEF;
  }
  if (pet.lck == 0) {
    pet.lck = PET_INITIAL_LCK;
  }
  if (pet.action != PET_STATE_SLEEP) {
    pet.action = PET_STATE_IDLE;
  }
  pet.arenaWins = 0;
  pet.arenaRunHp = pet.hp;

  displayEnabled = petPrefs.getBool("display", true);
}

void loadDashboardSettings() {
  weatherEnabled = petPrefs.getBool("weatherOn", DASHBOARD_WEATHER_ENABLED_DEFAULT != 0);
  proxmoxEnabled = petPrefs.getBool("pveOn", DASHBOARD_PROXMOX_ENABLED_DEFAULT != 0);

  uint8_t savedHomeInfoMode = petPrefs.getUChar("homeInfo", DASHBOARD_HOME_INFO_MODE_DEFAULT);
  if (savedHomeInfoMode > HOME_INFO_CLOCK) {
    savedHomeInfoMode = HOME_INFO_AUTO;
  }
  homeInfoMode = static_cast<HomeInfoMode>(savedHomeInfoMode);

  if (!weatherEnabled) {
    clearWeatherMetrics("off");
  }
  if (!proxmoxEnabled) {
    clearProxmoxMetrics("off");
  }
}

void savePetState() {
  petPrefs.putUChar("hunger", pet.hunger);
  petPrefs.putUChar("energy", pet.energy);
  petPrefs.putUChar("food", pet.food);
  petPrefs.putUChar("level", pet.level);
  petPrefs.putUShort("xp", pet.xp);
  petPrefs.putUChar("hp", pet.hp);
  petPrefs.putUChar("atk", pet.atk);
  petPrefs.putUChar("def", pet.def);
  petPrefs.putUChar("lck", pet.lck);
  petPrefs.putUShort("arenaBest", pet.arenaBest);
  petPrefs.putUShort("arenaLast", pet.arenaLast);
  petPrefs.putUChar("action", static_cast<uint8_t>(pet.action));
  petPrefs.putInt("wakeDay", pet.sleepWakeDay);
  petPrefs.putBool("display", displayEnabled);
}

void saveDashboardSettings() {
  petPrefs.putBool("weatherOn", weatherEnabled);
  petPrefs.putBool("pveOn", proxmoxEnabled);
  petPrefs.putUChar("homeInfo", static_cast<uint8_t>(homeInfoMode));
}

bool hasConnectionAlert() {
  const bool anyNetworkServiceEnabled = weatherEnabled || proxmoxEnabled;
  if (anyNetworkServiceEnabled && WiFi.status() != WL_CONNECTED) {
    return true;
  }

  return proxmoxEnabled && !proxmox.valid;
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

uint16_t xpForNextLevel() {
  return static_cast<uint16_t>(pet.level) * 10U;
}

uint16_t arenaTargetWins() {
  return 3U + static_cast<uint16_t>(pet.level) * 2U;
}

uint8_t petSkillRank() {
  if (pet.level >= 15) {
    return 5;
  }
  if (pet.level >= 12) {
    return 4;
  }
  if (pet.level >= 9) {
    return 3;
  }
  if (pet.level >= 6) {
    return 2;
  }
  if (pet.level >= 3) {
    return 1;
  }
  return 0;
}

uint8_t clampU8(uint16_t value) {
  return value > 255U ? 255U : static_cast<uint8_t>(value);
}

void applyLevelUpStats(uint8_t newLevel) {
  pet.hp = clampU8(static_cast<uint16_t>(pet.hp) + 2U);
  if (newLevel % 2 == 0) {
    pet.atk = clampU8(static_cast<uint16_t>(pet.atk) + 1U);
  }
  if (newLevel % 3 == 0) {
    pet.def = clampU8(static_cast<uint16_t>(pet.def) + 1U);
  }
  if (newLevel % 4 == 0) {
    pet.lck = clampU8(static_cast<uint16_t>(pet.lck) + 1U);
  }
}

void addPetXp(uint16_t amount) {
  if (pet.level >= PET_MAX_LEVEL) {
    pet.xp = 0;
    return;
  }

  pet.xp = static_cast<uint16_t>(pet.xp + amount);
  while (pet.level < PET_MAX_LEVEL && pet.xp >= xpForNextLevel()) {
    pet.xp = static_cast<uint16_t>(pet.xp - xpForNextLevel());
    pet.level++;
    applyLevelUpStats(pet.level);
  }

  if (pet.level >= PET_MAX_LEVEL) {
    pet.xp = 0;
  }
}

bool isPetBusy() {
  return pet.action == PET_STATE_HUNT || pet.action == PET_STATE_EAT || pet.action == PET_STATE_ARENA;
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

void startTaskCompleteLedPattern() {
  taskLedPatternActive = true;
  taskLedPatternStep = 0;
  taskLedPatternStepStartedMs = millis();
}

bool taskCompleteLedOn() {
  if (!taskLedPatternActive) {
    return false;
  }

  const uint32_t now = millis();
  while (taskLedPatternActive && now - taskLedPatternStepStartedMs >= TASK_LED_PATTERN_MS[taskLedPatternStep]) {
    taskLedPatternStepStartedMs += TASK_LED_PATTERN_MS[taskLedPatternStep];
    taskLedPatternStep++;
    if (taskLedPatternStep >= TASK_LED_PATTERN_STEP_COUNT) {
      taskLedPatternActive = false;
    }
  }

  return taskLedPatternActive && TASK_LED_PATTERN_ON[taskLedPatternStep];
}

uint8_t rollHuntFood() {
  const long roll = random(100);
  uint8_t foundFood = 0;

  if (roll < 20) {
    foundFood = 0;
  } else if (roll < 68) {
    foundFood = 1;
  } else if (roll < 92) {
    foundFood = 2;
  } else {
    foundFood = 4;
  }

  const uint16_t rawLckRerollChance = static_cast<uint16_t>(pet.lck) * 6U;
  const uint8_t lckRerollChance = rawLckRerollChance > 50U ? 50U : rawLckRerollChance;
  if (foundFood == 0 && random(100) < lckRerollChance) {
    foundFood = 1;
  }

  const uint16_t rawAtkBonusChance = static_cast<uint16_t>(pet.atk) * 4U;
  const uint8_t atkBonusChance = rawAtkBonusChance > 35U ? 35U : rawAtkBonusChance;
  if (foundFood > 0 && random(100) < atkBonusChance) {
    foundFood++;
  }

  return foundFood;
}

uint8_t rollHuntHungerCost() {
  uint8_t hungerCost = random(PET_HUNT_MAX_HUNGER_COST + 1);
  const uint16_t rawDefReduceChance = static_cast<uint16_t>(pet.def) * 8U;
  const uint8_t defReduceChance = rawDefReduceChance > 50U ? 50U : rawDefReduceChance;
  if (hungerCost > 0 && random(100) < defReduceChance) {
    const uint8_t reduction = 1 + (pet.def / 4);
    hungerCost = hungerCost > reduction ? hungerCost - reduction : 0;
  }
  return hungerCost;
}

void resetPetActionResult(PetResultType type) {
  petResultOpen = false;
  petResultType = type;
  petResultFoodDelta = 0;
  petResultHungerDelta = 0;
  petResultEnergyDelta = 0;
  petResultRuns = 0;
}

void addPetActionResultDelta(uint8_t oldFood, uint8_t oldHunger, uint8_t oldEnergy) {
  petResultFoodDelta += static_cast<int16_t>(pet.food) - static_cast<int16_t>(oldFood);
  petResultHungerDelta += static_cast<int16_t>(pet.hunger) - static_cast<int16_t>(oldHunger);
  petResultEnergyDelta += static_cast<int16_t>(pet.energy) - static_cast<int16_t>(oldEnergy);
  if (petResultRuns < 255) {
    petResultRuns++;
  }
}

void openPetActionResult(PetResultType type) {
  petResultType = type;
  petResultOpen = true;
  currentPage = PAGE_DASHBOARD;
}

void resetArenaResultSummary() {
  arenaResultOpen = false;
  arenaResultWins = 0;
  arenaResultXp = 0;
  arenaResultLevel = pet.level;
  arenaResultRuns = 0;
}

void resetArenaAnimationChoice() {
  arenaAnimationStepId = 0;
  arenaAnimationChoice = 0;
  arenaAnimationChoiceReady = false;
}

void resetPetToInitialState() {
  const bool currentDisplayEnabled = displayEnabled;
  pet = PetState();
  pet.actionStartedMs = millis();
  pet.statTickMs = millis();
  pet.arenaRunHp = pet.hp;
  displayEnabled = currentDisplayEnabled;

  huntRunsRemaining = 0;
  eatRunsRemaining = 0;
  arenaRunsRemaining = 0;
  resetPetActionResult(PET_RESULT_NONE);
  resetArenaResultSummary();
  resetArenaAnimationChoice();
  taskLedPatternActive = false;
  taskLedPatternStep = 0;
  taskLedPatternStepStartedMs = 0;
  digitalWrite(RED_LED_PIN, LOW);
  savePetState();
}

uint16_t calculateArenaXp(uint16_t wins) {
  if (wins == 0) {
    return 0;
  }

  const uint32_t numerator = static_cast<uint32_t>(xpForNextLevel()) * wins * PET_ARENA_XP_PERCENT_AT_TARGET;
  const uint32_t denominator = static_cast<uint32_t>(arenaTargetWins()) * 100UL;
  uint16_t xpGain = static_cast<uint16_t>((numerator + denominator - 1) / denominator);
  if (xpGain == 0) {
    xpGain = 1;
  }
  return xpGain;
}

void finishArenaRun() {
  const uint16_t runWins = pet.arenaWins;
  const uint16_t xpGain = calculateArenaXp(runWins);
  pet.arenaLast = runWins;
  if (runWins > pet.arenaBest) {
    pet.arenaBest = runWins;
  }
  addPetXp(xpGain);

  arenaResultWins += runWins;
  arenaResultXp += xpGain;
  arenaResultLevel = pet.level;
  arenaResultRuns++;
  currentPage = PAGE_DASHBOARD;

  pet.arenaWins = 0;
  pet.arenaRunHp = pet.hp;
  pet.action = PET_STATE_IDLE;
  pet.actionStartedMs = millis();
  pet.statTickMs = millis();

  if (arenaRunsRemaining > 0) {
    arenaRunsRemaining--;
    if (startPetAction(PET_STATE_ARENA)) {
      return;
    }
    arenaRunsRemaining = 0;
  }

  arenaResultOpen = true;
  startTaskCompleteLedPattern();
  savePetState();
}

void resolveArenaCombat() {
  const uint16_t enemyLevel = pet.arenaWins + 1U;
  const uint16_t enemyPressure = enemyLevel * 2U + random(enemyLevel + 2U);
  const uint8_t skillRank = petSkillRank();
  const uint16_t statGuard = static_cast<uint16_t>(pet.def) * 2U + pet.atk + random(static_cast<uint16_t>(pet.lck) + skillRank + 1U);
  int16_t hpLoss = static_cast<int16_t>(enemyPressure) - static_cast<int16_t>(statGuard / 2U) - skillRank;
  if (hpLoss < 1) {
    hpLoss = 1;
  }
  pet.arenaRunHp -= hpLoss;
  if (pet.arenaRunHp > 0) {
    pet.arenaWins++;
  }
}

void finishPetAction() {
  bool completedTask = false;

  switch (pet.action) {
  case PET_STATE_HUNT: {
    const uint8_t oldFood = pet.food;
    const uint8_t oldHunger = pet.hunger;
    const uint8_t oldEnergy = pet.energy;
    const uint8_t foundFood = rollHuntFood();
    const uint8_t hungerCost = rollHuntHungerCost();
    pet.energy = clampPetStat(static_cast<int16_t>(pet.energy) - PET_HUNT_ENERGY_COST);
    pet.hunger = clampPetStat(static_cast<int16_t>(pet.hunger) - hungerCost);
    pet.food = clampFood(static_cast<int16_t>(pet.food) + foundFood);
    addPetActionResultDelta(oldFood, oldHunger, oldEnergy);
    if (huntRunsRemaining > 0 && pet.energy >= PET_HUNT_MIN_ENERGY && pet.food < PET_MAX_FOOD) {
      huntRunsRemaining--;
      pet.action = PET_STATE_HUNT;
    } else {
      if (huntRunsRemaining > 0 && pet.food < PET_MAX_FOOD) {
        startPetNotice(PET_NOTICE_NO_ENERGY);
      }
      huntRunsRemaining = 0;
      pet.action = PET_STATE_IDLE;
      completedTask = true;
      openPetActionResult(PET_RESULT_HUNT);
    }
    break;
  }
  case PET_STATE_EAT: {
    const uint8_t oldFood = pet.food;
    const uint8_t oldHunger = pet.hunger;
    const uint8_t oldEnergy = pet.energy;
    const uint8_t energyCost = random(PET_EAT_MAX_ENERGY_COST + 1);
    pet.food = clampFood(static_cast<int16_t>(pet.food) - 1);
    pet.hunger = clampPetStat(static_cast<int16_t>(pet.hunger) + PET_EAT_HUNGER_GAIN);
    pet.energy = clampPetStat(static_cast<int16_t>(pet.energy) - energyCost);
    addPetActionResultDelta(oldFood, oldHunger, oldEnergy);
    if (eatRunsRemaining > 0 && pet.food > 0 && pet.hunger < 100) {
      eatRunsRemaining--;
      pet.action = PET_STATE_EAT;
    } else {
      eatRunsRemaining = 0;
      pet.action = PET_STATE_IDLE;
      completedTask = true;
      openPetActionResult(PET_RESULT_EAT);
    }
    break;
  }
  default:
    break;
  }

  pet.actionStartedMs = millis();
  pet.statTickMs = millis();
  if (completedTask) {
    startTaskCompleteLedPattern();
  }
  savePetState();
}

void cancelHuntAction() {
  if (pet.action != PET_STATE_HUNT) {
    return;
  }

  const uint8_t hungerCost = rollHuntHungerCost();
  pet.energy = clampPetStat(static_cast<int16_t>(pet.energy) - PET_HUNT_ENERGY_COST);
  pet.hunger = clampPetStat(static_cast<int16_t>(pet.hunger) - hungerCost);
  huntRunsRemaining = 0;
  pet.action = PET_STATE_IDLE;
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
    if (pet.food >= PET_MAX_FOOD) {
      startPetNotice(PET_NOTICE_FULL);
      return false;
    }
    if (pet.energy < PET_HUNT_MIN_ENERGY) {
      startPetNotice(PET_NOTICE_NO_ENERGY);
      return false;
    }
    resetPetActionResult(PET_RESULT_HUNT);
    break;
  case PET_STATE_ARENA:
    if (pet.energy < PET_ARENA_MIN_ENERGY) {
      startPetNotice(PET_NOTICE_NO_ENERGY);
      return false;
    }
    pet.energy = clampPetStat(static_cast<int16_t>(pet.energy) - PET_ARENA_ENERGY_COST);
    pet.hunger = clampPetStat(static_cast<int16_t>(pet.hunger) - PET_ARENA_HUNGER_COST);
    pet.arenaWins = 0;
    pet.arenaRunHp = pet.hp;
    resetArenaAnimationChoice();
    break;
  case PET_STATE_EAT:
    if (pet.food == 0) {
      startPetNotice(PET_NOTICE_NO_FOOD);
      return false;
    }
    if (pet.hunger >= 100) {
      startPetNotice(PET_NOTICE_FULL);
      return false;
    }
    resetPetActionResult(PET_RESULT_EAT);
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
    pet.hunger = clampPetStat(static_cast<int16_t>(pet.hunger) - PET_SLEEP_HUNGER_LOSS_PER_HOUR);
    break;
  default:
    break;
  }
}

void applySleepEnergyTick() {
  if (pet.energy >= 100) {
    return;
  }
  pet.energy = clampPetStat(static_cast<int16_t>(pet.energy) + PET_SLEEP_ENERGY_GAIN);
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

  while (pet.action == PET_STATE_ARENA && now - pet.actionStartedMs >= PET_ARENA_COMBAT_INTERVAL_MS) {
    pet.actionStartedMs += PET_ARENA_COMBAT_INTERVAL_MS;
    resolveArenaCombat();
    changed = true;
    if (pet.arenaRunHp <= 0) {
      finishArenaRun();
      return;
    }
  }

  while (pet.action == PET_STATE_SLEEP && pet.energy < 100 && now - pet.actionStartedMs >= PET_SLEEP_ENERGY_TICK_MS) {
    pet.actionStartedMs += PET_SLEEP_ENERGY_TICK_MS;
    applySleepEnergyTick();
    changed = true;
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
  if (taskLedPatternActive) {
    const bool patternOn = taskCompleteLedOn();
    if (taskLedPatternActive) {
      digitalWrite(RED_LED_PIN, patternOn ? HIGH : LOW);
      return;
    }
  }

  digitalWrite(RED_LED_PIN, activePetNotice() == PET_NOTICE_HOT ? HIGH : LOW);
}

uint8_t maxHuntRunCount() {
  if (pet.food >= PET_MAX_FOOD || PET_HUNT_ENERGY_COST == 0 || pet.energy < PET_HUNT_MIN_ENERGY) {
    return 0;
  }

  const uint16_t maxRuns = pet.energy / PET_HUNT_ENERGY_COST;
  return maxRuns > 100U ? 100 : maxRuns;
}

String huntRunMenuLabel(uint8_t index) {
  if (index == 0) {
    return "E 0/" + String(pet.energy);
  }

  const uint8_t runs = index;
  const uint16_t energyCost = static_cast<uint16_t>(runs) * PET_HUNT_ENERGY_COST;

  return "E " + String(energyCost) + "/" + String(pet.energy);
}

void startHuntRunQueue(uint8_t runs) {
  if (runs == 0) {
    return;
  }

  huntRunsRemaining = runs - 1;
  if (!startPetAction(PET_STATE_HUNT)) {
    huntRunsRemaining = 0;
  }
}

uint8_t maxEatRunCount() {
  if (pet.food == 0 || pet.hunger >= 100) {
    return 0;
  }

  const uint8_t missingHunger = 100 - pet.hunger;
  const uint8_t usefulRuns = (missingHunger + PET_EAT_HUNGER_GAIN - 1) / PET_EAT_HUNGER_GAIN;
  return pet.food < usefulRuns ? pet.food : usefulRuns;
}

String eatRunMenuLabel(uint8_t index) {
  if (index == 0) {
    return "F 0/" + String(pet.food);
  }

  return "F " + String(index) + "/" + String(pet.food);
}

void startEatRunQueue(uint8_t runs) {
  if (runs == 0) {
    return;
  }

  eatRunsRemaining = runs - 1;
  if (!startPetAction(PET_STATE_EAT)) {
    eatRunsRemaining = 0;
  }
}

uint8_t maxArenaRunCount() {
  if (PET_ARENA_ENERGY_COST == 0 || pet.energy < PET_ARENA_MIN_ENERGY) {
    return 0;
  }

  const uint16_t maxRuns = pet.energy / PET_ARENA_ENERGY_COST;
  return maxRuns > 100U ? 100 : maxRuns;
}

String arenaRunMenuLabel(uint8_t index) {
  if (index == 0) {
    return "E 0/" + String(pet.energy) + " H 0/" + String(pet.hunger);
  }

  const uint16_t energyCost = static_cast<uint16_t>(index) * PET_ARENA_ENERGY_COST;
  const uint16_t hungerCost = static_cast<uint16_t>(index) * PET_ARENA_HUNGER_COST;
  return "E " + String(energyCost) + "/" + String(pet.energy) + " H " + String(hungerCost) + "/" + String(pet.hunger);
}

void startArenaRunQueue(uint8_t runs) {
  if (runs == 0) {
    return;
  }

  resetArenaResultSummary();
  arenaRunsRemaining = runs - 1;
  if (!startPetAction(PET_STATE_ARENA)) {
    arenaRunsRemaining = 0;
  }
}

uint8_t activityMenuActionCount() {
  if (pet.action == PET_STATE_HUNT || pet.action == PET_STATE_SLEEP) {
    return 4;
  }

  return 3;
}

String activityMenuLabel(uint8_t index) {
  if (index == 0) {
    return "STATS";
  }

  uint8_t actionIndex = 1;
  if (pet.action == PET_STATE_HUNT) {
    if (index == actionIndex) {
      return "STOP";
    }
    actionIndex++;
  } else if (pet.action == PET_STATE_SLEEP) {
    if (index == actionIndex) {
      return "WAKE UP";
    }
    actionIndex++;
  }

  if (index == actionIndex) {
    return "SCREEN OFF";
  }
  if (index == actionIndex + 1) {
    return "BACK";
  }

  return "";
}

String onOffLabel(bool enabled) {
  return enabled ? "ON" : "OFF";
}

String homeInfoModeLabel() {
  switch (homeInfoMode) {
  case HOME_INFO_TEMP:
    return "TEMP";
  case HOME_INFO_CLOCK:
    return "CLOCK";
  case HOME_INFO_AUTO:
  default:
    return "AUTO";
  }
}

String settingsMenuLabel(uint8_t index) {
  switch (index) {
  case 0:
    return "WEATHER " + onOffLabel(weatherEnabled);
  case 1:
    return "PROXMOX " + onOffLabel(proxmoxEnabled);
  case 2:
    return "HOME " + homeInfoModeLabel();
  case 3:
    return "BACK";
  default:
    return "";
  }
}

void openPetStats() {
  petMenuOpen = false;
  petMenuMode = PET_MENU_MAIN;
  petSheetOpen = true;
  petSheetPage = 0;
  currentPage = PAGE_DASHBOARD;
}

void turnScreenOff() {
  petMenuOpen = false;
  petMenuMode = PET_MENU_MAIN;
  displayEnabled = false;
  oled.setPowerSave(1);
  savePetState();
}

String durationLabel(uint32_t durationMs) {
  const uint16_t minutes = (durationMs + 59999UL) / 60000UL;

  if (minutes >= 60) {
    const uint16_t hours = minutes / 60;
    const uint16_t remainingMinutes = minutes % 60;
    if (remainingMinutes == 0) {
      return String(hours) + "H";
    }
    return String(hours) + "H" + String(remainingMinutes);
  }

  return String(minutes) + "M";
}

String petMenuLabel(uint8_t index) {
  if (petMenuMode == PET_MENU_ACTIVITY) {
    return activityMenuLabel(index);
  }

  if (petMenuMode == PET_MENU_HUNT_RUNS) {
    return huntRunMenuLabel(index);
  }

  if (petMenuMode == PET_MENU_EAT_RUNS) {
    return eatRunMenuLabel(index);
  }

  if (petMenuMode == PET_MENU_ARENA_RUNS) {
    return arenaRunMenuLabel(index);
  }

  if (petMenuMode == PET_MENU_RESET_CONFIRM) {
    return index == 0 ? "CANCEL" : "CONFIRM";
  }

  if (petMenuMode == PET_MENU_SETTINGS) {
    return settingsMenuLabel(index);
  }

  if (isPetSleeping()) {
    switch (index) {
    case 0:
      return "WAKE UP";
    case 1:
      return "SCREEN OFF";
    case 2:
      return "BACK";
    default:
      return "";
    }
  }

  switch (index) {
  case PET_ACTION_HUNT:
    return "HUNT " + durationLabel(PET_HUNT_DURATION_MS);
  case PET_ACTION_EAT:
    return "EAT " + durationLabel(PET_EAT_DURATION_MS);
  case PET_ACTION_SLEEP:
    return "SLEEP";
  case PET_ACTION_ARENA:
    return "ARENA";
  case PET_ACTION_SHEET:
    return "STATS";
  case PET_ACTION_SETTINGS:
    return "SETTINGS";
  case PET_ACTION_SCREEN_OFF:
    return "SCREEN OFF";
  case PET_ACTION_RESET:
    return "RESET PET";
  case PET_ACTION_BACK:
    return "BACK";
  default:
    return "";
  }
}

uint8_t petMenuActionCount() {
  if (petMenuMode == PET_MENU_ACTIVITY) {
    return activityMenuActionCount();
  }

  if (petMenuMode == PET_MENU_HUNT_RUNS) {
    const uint8_t maxRuns = maxHuntRunCount();
    return maxRuns + 1;
  }

  if (petMenuMode == PET_MENU_EAT_RUNS) {
    const uint8_t maxRuns = maxEatRunCount();
    return maxRuns + 1;
  }

  if (petMenuMode == PET_MENU_ARENA_RUNS) {
    const uint8_t maxRuns = maxArenaRunCount();
    return maxRuns + 1;
  }

  if (petMenuMode == PET_MENU_RESET_CONFIRM) {
    return 2;
  }

  if (petMenuMode == PET_MENU_SETTINGS) {
    return 4;
  }

  return isPetSleeping() ? 3 : PET_ACTION_COUNT;
}

void toggleWeatherEnabled() {
  weatherEnabled = !weatherEnabled;
  if (weatherEnabled) {
    weatherPollRequested = true;
  } else {
    weatherPollRequested = false;
    clearWeatherMetrics("off");
  }
  saveDashboardSettings();
}

void toggleProxmoxEnabled() {
  proxmoxEnabled = !proxmoxEnabled;
  if (proxmoxEnabled) {
    proxmoxPollRequested = true;
  } else {
    proxmoxPollRequested = false;
    clearProxmoxMetrics("off");
    if (currentPage >= 2) {
      currentPage = PAGE_DASHBOARD;
    }
  }
  saveDashboardSettings();
}

void cycleHomeInfoMode() {
  switch (homeInfoMode) {
  case HOME_INFO_AUTO:
    homeInfoMode = HOME_INFO_TEMP;
    break;
  case HOME_INFO_TEMP:
    homeInfoMode = HOME_INFO_CLOCK;
    break;
  case HOME_INFO_CLOCK:
  default:
    homeInfoMode = HOME_INFO_AUTO;
    break;
  }
  saveDashboardSettings();
}

void executePetAction() {
  if (petMenuMode == PET_MENU_ACTIVITY) {
    if (petMenuIndex == 0) {
      openPetStats();
      return;
    }

    uint8_t actionIndex = 1;
    if (pet.action == PET_STATE_HUNT) {
      if (petMenuIndex == actionIndex) {
        petMenuOpen = false;
        petMenuMode = PET_MENU_MAIN;
        cancelHuntAction();
        return;
      }
      actionIndex++;
    } else if (pet.action == PET_STATE_SLEEP) {
      if (petMenuIndex == actionIndex) {
        petMenuOpen = false;
        petMenuMode = PET_MENU_MAIN;
        wakePet();
        return;
      }
      actionIndex++;
    }

    if (petMenuIndex == actionIndex) {
      turnScreenOff();
      return;
    }

    petMenuOpen = false;
    petMenuMode = PET_MENU_MAIN;
    return;
  }

  if (petMenuMode == PET_MENU_HUNT_RUNS) {
    const uint8_t runs = petMenuIndex;
    petMenuOpen = false;
    petMenuMode = PET_MENU_MAIN;
    if (runs > 0) {
      startHuntRunQueue(runs);
    }
    return;
  }

  if (petMenuMode == PET_MENU_EAT_RUNS) {
    const uint8_t runs = petMenuIndex;
    petMenuOpen = false;
    petMenuMode = PET_MENU_MAIN;
    if (runs > 0) {
      startEatRunQueue(runs);
    }
    return;
  }

  if (petMenuMode == PET_MENU_ARENA_RUNS) {
    const uint8_t runs = petMenuIndex;
    petMenuOpen = false;
    petMenuMode = PET_MENU_MAIN;
    if (runs > 0) {
      startArenaRunQueue(runs);
    }
    return;
  }

  if (petMenuMode == PET_MENU_RESET_CONFIRM) {
    if (petMenuIndex == 0) {
      petMenuMode = PET_MENU_MAIN;
      petMenuIndex = PET_ACTION_RESET;
      return;
    }

    resetPetToInitialState();
    petMenuOpen = false;
    petMenuMode = PET_MENU_MAIN;
    petMenuIndex = PET_ACTION_SHEET;
    currentPage = PAGE_DASHBOARD;
    return;
  }

  if (petMenuMode == PET_MENU_SETTINGS) {
    switch (petMenuIndex) {
    case 0:
      toggleWeatherEnabled();
      break;
    case 1:
      toggleProxmoxEnabled();
      break;
    case 2:
      cycleHomeInfoMode();
      break;
    case 3:
    default:
      petMenuMode = PET_MENU_MAIN;
      petMenuIndex = PET_ACTION_SETTINGS;
      break;
    }
    return;
  }

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
    case 2:
      petMenuOpen = false;
      break;
    default:
      petMenuOpen = false;
      break;
    }
    return;
  }

  switch (petMenuIndex) {
  case PET_ACTION_HUNT: {
    const uint8_t maxRuns = maxHuntRunCount();
    if (maxRuns == 0) {
      petMenuOpen = false;
      startPetAction(PET_STATE_HUNT);
      break;
    }
    petMenuMode = PET_MENU_HUNT_RUNS;
    petMenuIndex = 1;
    break;
  }
  case PET_ACTION_EAT: {
    const uint8_t maxRuns = maxEatRunCount();
    if (maxRuns == 0) {
      petMenuOpen = false;
      startPetAction(PET_STATE_EAT);
      break;
    }
    petMenuMode = PET_MENU_EAT_RUNS;
    petMenuIndex = 1;
    break;
  }
  case PET_ACTION_SLEEP:
    petMenuOpen = false;
    startPetAction(PET_STATE_SLEEP);
    break;
  case PET_ACTION_ARENA:
    if (maxArenaRunCount() == 0) {
      petMenuOpen = false;
      startPetAction(PET_STATE_ARENA);
      break;
    }
    petMenuMode = PET_MENU_ARENA_RUNS;
    petMenuIndex = 1;
    break;
  case PET_ACTION_SHEET:
    openPetStats();
    break;
  case PET_ACTION_SETTINGS:
    petMenuMode = PET_MENU_SETTINGS;
    petMenuIndex = 0;
    break;
  case PET_ACTION_SCREEN_OFF:
    turnScreenOff();
    break;
  case PET_ACTION_RESET:
    petMenuMode = PET_MENU_RESET_CONFIRM;
    petMenuIndex = 0;
    break;
  case PET_ACTION_BACK:
    petMenuOpen = false;
    break;
  default:
    break;
  }
}

uint8_t pageCount() {
  if (!proxmoxEnabled) {
    return 2;
  }
  return BASE_PAGE_COUNT + proxmox.guestCount;
}

uint8_t networkPageIndex() {
  return proxmoxEnabled ? PAGE_NETWORK : 1;
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
  const bool nextPressed = consumeButtonPress(nextButtonPending, lastNextPressMs);
  const bool selectPressed = consumeButtonPress(selectButtonPending, lastSelectPressMs);
  const bool previousPressed = consumeButtonPress(previousButtonPending, lastPreviousPressMs);

  if (!displayEnabled) {
    if (nextPressed || selectPressed || previousPressed) {
      displayEnabled = true;
      oled.setPowerSave(0);
      savePetState();
      Serial.println("BTN -> display on");
    }
    return;
  }

  if (arenaResultOpen) {
    if (selectPressed) {
      arenaResultOpen = false;
    }
    return;
  }

  if (petResultOpen) {
    if (selectPressed) {
      petResultOpen = false;
      petResultType = PET_RESULT_NONE;
    }
    return;
  }

  if (petSheetOpen) {
    if (nextPressed) {
      petSheetPage = (petSheetPage + 1) % PET_SHEET_PAGE_COUNT;
    }
    if (previousPressed) {
      petSheetPage = petSheetPage == 0 ? PET_SHEET_PAGE_COUNT - 1 : petSheetPage - 1;
    }
    if (selectPressed) {
      petSheetOpen = false;
    }
    return;
  }

  if (petMenuOpen) {
    if (petMenuMode == PET_MENU_HUNT_RUNS || petMenuMode == PET_MENU_EAT_RUNS || petMenuMode == PET_MENU_ARENA_RUNS) {
      const uint8_t maxIndex = petMenuActionCount() - 1;
      if (nextPressed && petMenuIndex < maxIndex) {
        petMenuIndex++;
      }

      if (previousPressed && petMenuIndex > 0) {
        petMenuIndex--;
      }

      if (selectPressed) {
        executePetAction();
      }

      return;
    }

    if (nextPressed) {
      petMenuIndex = (petMenuIndex + 1) % petMenuActionCount();
    }

    if (previousPressed) {
      const uint8_t actionCount = petMenuActionCount();
      petMenuIndex = petMenuIndex == 0 ? actionCount - 1 : petMenuIndex - 1;
    }

    if (selectPressed) {
      executePetAction();
    }

    return;
  }

  if (selectPressed) {
    if (currentPage == PAGE_DASHBOARD) {
      if (isPetBusy() || isPetSleeping()) {
        petMenuOpen = true;
        petMenuMode = PET_MENU_ACTIVITY;
        petMenuIndex = 0;
        Serial.println("BTN SELECT -> activity menu");
      } else {
        petMenuOpen = true;
        petMenuMode = PET_MENU_MAIN;
        petMenuIndex = 0;
        Serial.println("BTN SELECT -> pet menu");
      }
    } else {
      currentPage = PAGE_DASHBOARD;
      Serial.println("BTN SELECT -> home");
    }
  }

  if (nextPressed) {
    nextPage();
    petMenuOpen = false;
    petMenuMode = PET_MENU_MAIN;
    Serial.print("BTN NEXT -> page ");
    Serial.println(static_cast<uint8_t>(currentPage) + 1);
  }

  if (previousPressed) {
    previousPage();
    petMenuOpen = false;
    petMenuMode = PET_MENU_MAIN;
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

void drawLargeTemperature(uint8_t x, uint8_t baseline, float temperature) {
  oled.setFont(u8g2_font_7x14B_tf);
  if (isnan(temperature)) {
    oled.setCursor(x, baseline);
    oled.print("--.-C");
    return;
  }

  const String value = String(temperature, 1) + "C";
  oled.setCursor(x, baseline);
  oled.print(value);
  oled.drawCircle(x + oled.getStrWidth(value.c_str()) - 7, baseline - 10, 1);
}

void drawLargeRightAlignedTemperature(uint8_t rightX, uint8_t baseline, float temperature) {
  oled.setFont(u8g2_font_7x14B_tf);
  const String value = isnan(temperature) ? "--.-C" : String(temperature, 1) + "C";
  const int width = oled.getStrWidth(value.c_str());
  const int x = rightX - width + 1;
  oled.setCursor(x, baseline);
  oled.print(value);
  oled.drawCircle(x + width - 7, baseline - 10, 1);
}

void drawLargeRightAlignedText(uint8_t rightX, uint8_t baseline, const String &value) {
  oled.setFont(u8g2_font_7x14B_tf);
  const int width = oled.getStrWidth(value.c_str());
  oled.setCursor(rightX - width + 1, baseline);
  oled.print(value);
}

String homeClockLabel() {
  const String clock = formatClockFromNtp();
  return clock.length() > 0 ? clock : "--:--";
}

void drawHomeInfo(uint8_t rightX, uint8_t baseline) {
  if (homeInfoMode == HOME_INFO_TEMP) {
    drawLargeRightAlignedTemperature(rightX, baseline, weatherEnabled ? weather.temperature : NAN);
    return;
  }

  if (homeInfoMode == HOME_INFO_CLOCK) {
    drawLargeRightAlignedText(rightX, baseline, homeClockLabel());
    return;
  }

  if (weatherEnabled && weather.valid && !isnan(weather.temperature)) {
    drawLargeRightAlignedTemperature(rightX, baseline, weather.temperature);
    return;
  }

  drawLargeRightAlignedText(rightX, baseline, homeClockLabel());
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
  static const uint8_t fRows[] = {0b111, 0b100, 0b110, 0b100, 0b100};

  if (label == 'E') {
    rows = eRows;
  } else if (label == 'H') {
    rows = hRows;
  } else if (label == 'F') {
    rows = fRows;
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

void drawContinuousStatBar(uint8_t x, uint8_t y, char label, uint8_t value, bool blinkFill = false) {
  constexpr uint8_t labelWidth = 3;
  constexpr uint8_t labelGap = 4;
  constexpr uint8_t barWidth = 30;
  constexpr uint8_t barHeight = 5;
  constexpr uint8_t fillMaxWidth = barWidth - 2;

  drawPixelStatLabel(x, y, label);

  const uint8_t clampedValue = value > 100 ? 100 : value;
  const uint8_t barX = x + labelWidth + labelGap;
  const uint8_t fillWidth = clampedValue == 0 ? 0 : (clampedValue * fillMaxWidth + 99) / 100;

  oled.drawFrame(barX, y, barWidth, barHeight);
  if (blinkFill && ((millis() / 500UL) % 2UL) != 0) {
    return;
  }
  if (fillWidth > 0) {
    oled.drawBox(barX + 1, y + 1, fillWidth, barHeight - 2);
  }
}

void drawPetGauge(uint8_t x, uint8_t y, char label, uint8_t value) {
  oled.setFont(u8g2_font_4x6_tf);
  oled.setCursor(x, y + 5);
  oled.print(label);
  drawTinyBar(x + 6, y + 2, 22, value);
}

void drawRunSelectorMenu() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tf);

  const char *title = "ARENA RUNS";
  if (petMenuMode == PET_MENU_HUNT_RUNS) {
    title = "HUNT RUNS";
  } else if (petMenuMode == PET_MENU_EAT_RUNS) {
    title = "EAT RUNS";
  }
  const int titleWidth = oled.getStrWidth(title);
  oled.setCursor((128 - titleWidth) / 2, 7);
  oled.print(title);

  const String value = petMenuIndex == 0 ? "< Cancel >" : "< " + String(petMenuIndex) + " >";
  oled.setFont(u8g2_font_7x14B_tf);
  const int valueWidth = oled.getStrWidth(value.c_str());
  oled.setCursor((128 - valueWidth) / 2, 31);
  oled.print(value);

  String energyLabel = arenaRunMenuLabel(petMenuIndex);
  if (petMenuMode == PET_MENU_HUNT_RUNS) {
    energyLabel = huntRunMenuLabel(petMenuIndex);
  } else if (petMenuMode == PET_MENU_EAT_RUNS) {
    energyLabel = eatRunMenuLabel(petMenuIndex);
  }
  oled.setFont(u8g2_font_6x12_tf);
  const int energyWidth = oled.getStrWidth(energyLabel.c_str());
  oled.setCursor((128 - energyWidth) / 2, 50);
  oled.print(energyLabel);
}

void drawResetConfirmMenu() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tf);

  const char *title = "RESET PET";
  const int titleWidth = oled.getStrWidth(title);
  oled.setCursor((128 - titleWidth) / 2, 7);
  oled.print(title);

  oled.setFont(u8g2_font_6x12_tf);
  oled.setCursor(8, 22);
  oled.print("DELETE PET?");
  oled.setCursor(8, 34);
  oled.print("RESET STATE");

  for (uint8_t index = 0; index < 2; index++) {
    const bool selected = index == petMenuIndex;
    const uint8_t x = index == 0 ? 5 : 69;
    const char *label = index == 0 ? "CANCEL" : "CONFIRM";

    if (selected) {
      oled.drawBox(x - 2, 48, 58, 12);
      oled.setDrawColor(0);
    }

    oled.setCursor(x + 2, 58);
    oled.print(label);

    if (selected) {
      oled.setDrawColor(1);
    }
  }
}

void drawPetMenu() {
  if (petMenuMode == PET_MENU_HUNT_RUNS || petMenuMode == PET_MENU_EAT_RUNS || petMenuMode == PET_MENU_ARENA_RUNS) {
    drawRunSelectorMenu();
    return;
  }

  if (petMenuMode == PET_MENU_RESET_CONFIRM) {
    drawResetConfirmMenu();
    return;
  }

  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tf);

  const char *title = "MENU";
  if (petMenuMode == PET_MENU_HUNT_RUNS) {
    title = "HUNT RUNS";
  } else if (petMenuMode == PET_MENU_EAT_RUNS) {
    title = "EAT RUNS";
  } else if (petMenuMode == PET_MENU_ARENA_RUNS) {
    title = "ARENA RUNS";
  } else if (petMenuMode == PET_MENU_ACTIVITY) {
    title = "ACTIVITY";
  } else if (petMenuMode == PET_MENU_SETTINGS) {
    title = "SETTINGS";
  } else if (isPetSleeping()) {
    title = "SLEEP MENU";
  }
  const int titleWidth = oled.getStrWidth(title);
  oled.setCursor((128 - titleWidth) / 2, 7);
  oled.print(title);

  const uint8_t actionCount = petMenuActionCount();
  constexpr uint8_t visibleRows = 5;
  uint8_t firstIndex = 0;
  if (actionCount > visibleRows) {
    if (petMenuIndex <= 2) {
      firstIndex = 0;
    } else if (petMenuIndex >= actionCount - 3) {
      firstIndex = actionCount - visibleRows;
    } else {
      firstIndex = petMenuIndex - 2;
    }
  }

  oled.setFont(u8g2_font_6x12_tf);
  for (uint8_t row = 0; row < visibleRows && firstIndex + row < actionCount; row++) {
    const uint8_t actionIndex = firstIndex + row;
    const uint8_t baseline = 19 + row * 10;
    const bool selected = actionIndex == petMenuIndex;

    if (selected) {
      oled.drawBox(0, baseline - 9, 128, 11);
      oled.setDrawColor(0);
    }

    oled.setCursor(4, baseline);
    oled.print(selected ? ">" : " ");
    oled.setCursor(16, baseline);
    oled.print(petMenuLabel(actionIndex));

    if (selected) {
      oled.setDrawColor(1);
    }
  }

  oled.setFont(u8g2_font_5x8_tf);
  if (firstIndex > 0) {
    oled.setCursor(118, 7);
    oled.print("^");
  }
  if (firstIndex + visibleRows < actionCount) {
    oled.setCursor(118, 64);
    oled.print("v");
  }
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

String sleepEnergyRemainingLabel() {
  if (pet.action != PET_STATE_SLEEP || pet.energy >= 100) {
    return "";
  }

  const uint32_t elapsedMs = millis() - pet.actionStartedMs;
  const uint32_t remainingMs = elapsedMs >= PET_SLEEP_ENERGY_TICK_MS ? 0 : PET_SLEEP_ENERGY_TICK_MS - elapsedMs;
  return durationLabel(remainingMs);
}

uint32_t currentActionDurationMs() {
  switch (pet.action) {
  case PET_STATE_HUNT:
    return PET_HUNT_DURATION_MS;
  case PET_STATE_EAT:
    return PET_EAT_DURATION_MS;
  default:
    return 0;
  }
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

  if (notice == PET_NOTICE_FULL) {
    return "FULL";
  }

  if (notice == PET_NOTICE_BUSY) {
    return "BUSY";
  }

  switch (pet.action) {
  case PET_STATE_HUNT:
    return "HUNT " + actionRemainingLabel();
  case PET_STATE_ARENA:
    if (arenaRunsRemaining > 0) {
      return "ARENA x" + String(arenaRunsRemaining + 1);
    }
    return "ARENA";
  case PET_STATE_EAT:
    return "EAT " + actionRemainingLabel();
  case PET_STATE_SLEEP:
    if (pet.energy < 100) {
      return "SLEEP " + sleepEnergyRemainingLabel();
    }
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
    return "chill";
  default:
    return "";
  }
}

String petActivityLabel() {
  switch (pet.action) {
  case PET_STATE_HUNT:
    if (huntRunsRemaining > 0) {
      return "HUNT x" + String(huntRunsRemaining + 1);
    }
    return "HUNT";
  case PET_STATE_ARENA:
    if (arenaRunsRemaining > 0) {
      return "ARENA x" + String(arenaRunsRemaining + 1);
    }
    return "ARENA";
  case PET_STATE_EAT:
    if (eatRunsRemaining > 0) {
      return "EAT x" + String(eatRunsRemaining + 1);
    }
    return "EAT";
  case PET_STATE_SLEEP:
    return "SLEEP";
  default:
    return petMessage();
  }
}

void drawCountdownPie(uint8_t x, uint8_t y) {
  const uint32_t durationMs = currentActionDurationMs();
  if (durationMs == 0) {
    return;
  }

  const uint32_t elapsedMs = millis() - pet.actionStartedMs;
  const uint32_t remainingMs = elapsedMs >= durationMs ? 0 : durationMs - elapsedMs;
  uint8_t remainingSlices = (remainingMs * 8UL + durationMs - 1) / durationMs;
  if (remainingSlices > 8) {
    remainingSlices = 8;
  }

  // Remaining filled area from 0/8 to 8/8; the empty wedge consumes clockwise.
  const uint8_t masks[9][7] = {
      {0b0000000, 0b0000000, 0b0000000, 0b0000000, 0b0000000, 0b0000000, 0b0000000},
      {0b0001000, 0b0001100, 0b0001000, 0b0000000, 0b0000000, 0b0000000, 0b0000000},
      {0b0001100, 0b0001110, 0b0001111, 0b0001111, 0b0000000, 0b0000000, 0b0000000},
      {0b0001100, 0b0001110, 0b0001111, 0b0001111, 0b0000111, 0b0000110, 0b0000100},
      {0b0001100, 0b0001110, 0b0001111, 0b0001111, 0b0001111, 0b0001110, 0b0001100},
      {0b0001100, 0b0001110, 0b0001111, 0b0001111, 0b1111111, 0b0111110, 0b0011100},
      {0b0001100, 0b0001110, 0b0001111, 0b1111111, 0b1111111, 0b0111110, 0b0011100},
      {0b0001100, 0b0011110, 0b0111111, 0b1111111, 0b1111111, 0b0111110, 0b0011100},
      {0b0011100, 0b0111110, 0b1111111, 0b1111111, 0b1111111, 0b0111110, 0b0011100},
  };

  for (uint8_t row = 0; row < 7; row++) {
    for (uint8_t col = 0; col < 7; col++) {
      if ((masks[remainingSlices][row] & (1 << col)) != 0) {
        oled.drawPixel(x + col, y + row);
      }
    }
  }
}

void drawArenaWins(uint8_t x, uint8_t y) {
  oled.setFont(u8g2_font_5x8_tf);
  oled.setCursor(x, y + 8);
  oled.print("W");
  oled.print(pet.arenaWins);
}

void drawActivityPanel(uint8_t x, uint8_t y, const String &message) {
  if (message.length() == 0) {
    return;
  }

  oled.setFont(u8g2_font_6x12_tf);
  const int textWidth = oled.getStrWidth(message.c_str());
  uint8_t panelWidth = textWidth + 8;
  if (panelWidth < 24) {
    panelWidth = 24;
  }
  if (panelWidth > 58) {
    panelWidth = 58;
  }

  oled.drawFrame(x, y, panelWidth, 15);
  oled.setCursor(x + (panelWidth - textWidth) / 2, y + 11);
  oled.print(message);
  if (pet.action == PET_STATE_ARENA) {
    drawArenaWins(x + panelWidth + 2, y + 3);
  } else {
    String remaining = actionRemainingLabel();
    if (pet.action == PET_STATE_SLEEP) {
      remaining = sleepEnergyRemainingLabel();
    }
    if (remaining.length() > 0) {
      oled.setCursor(x + panelWidth + 6, y + 11);
      oled.print(remaining);
    }
  }
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

  if (pet.action == PET_STATE_ARENA) {
    const uint16_t attackMs = TAMAGOTCHI_BAT_FRAME_COUNT * PET_ANIMATION_FRAME_MS;
    const uint16_t attackStepMs = attackMs + PET_ARENA_ATTACK_PAUSE_MS;
    const uint32_t currentStepId = now / attackStepMs;
    const uint16_t attackFrameMs = now % attackStepMs;
    const uint8_t attackFrame = attackFrameMs < attackMs
                                  ? attackFrameMs / PET_ANIMATION_FRAME_MS
                                  : 0;
    if (!arenaAnimationChoiceReady || arenaAnimationStepId != currentStepId) {
      arenaAnimationStepId = currentStepId;
      arenaAnimationChoice = static_cast<uint8_t>(random(PET_ARENA_RANDOM_ANIMATION_COUNT));
      arenaAnimationChoiceReady = true;
    }

    PetRenderSprite sprite;
    sprite.x = PET_X_LEFT;
    if (arenaAnimationChoice < TAMAGOTCHI_BAT_ATTACK_COUNT) {
      sprite.bitmap = tmg_bat_fighting_attack_rows[arenaAnimationChoice][attackFrame];
    } else {
      sprite.bitmap = tmg_bat_normal_hurt_frames[attackFrame];
    }
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

  if (pet.action == PET_STATE_HUNT || pet.action == PET_STATE_ARENA) {
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

const char *petTitleLabel() {
  if (pet.level >= 20) {
    return "ELDER BAT";
  }
  if (pet.level >= 15) {
    return "FANG BAT";
  }
  if (pet.level >= 10) {
    return "NIGHT BAT";
  }
  if (pet.level >= 5) {
    return "CAVE BAT";
  }
  return "TINY BAT";
}

const char *petSkillLabel() {
  switch (petSkillRank()) {
  case 5:
    return "NIGHT MASTER";
  case 4:
    return "BLOOD FANG";
  case 3:
    return "ECHO";
  case 2:
    return "HARD WING";
  case 1:
    return "BITE";
  default:
    return "NONE";
  }
}

void drawArenaResultModal() {
  if (!arenaResultOpen) {
    return;
  }

  oled.drawBox(12, 7, 104, 54);
  oled.setDrawColor(0);
  oled.setFont(u8g2_font_6x12_tf);
  oled.setCursor(25, 18);
  oled.print("ARENA END");
  oled.setCursor(24, 30);
  oled.print("RUNS ");
  oled.print(arenaResultRuns);
  oled.print(" W ");
  oled.print(arenaResultWins);
  oled.setCursor(24, 42);
  oled.print("XP +");
  oled.print(arenaResultXp);
  oled.setCursor(24, 54);
  oled.print("LV ");
  oled.print(arenaResultLevel);
  oled.setCursor(75, 54);
  oled.print("SELECT");
  oled.setDrawColor(1);
}

void drawSignedDelta(int16_t value) {
  if (value > 0) {
    oled.print("+");
  }
  oled.print(value);
}

void drawPetResultModal() {
  if (!petResultOpen || petResultType == PET_RESULT_NONE) {
    return;
  }

  const char *title = petResultType == PET_RESULT_HUNT ? "HUNT END" : "EAT END";
  oled.drawBox(12, 7, 104, 54);
  oled.setDrawColor(0);
  oled.setFont(u8g2_font_6x12_tf);
  const int titleWidth = oled.getStrWidth(title);
  oled.setCursor(12 + (104 - titleWidth) / 2, 18);
  oled.print(title);

  oled.setCursor(24, 30);
  oled.print("RUNS ");
  oled.print(petResultRuns);

  oled.setCursor(24, 42);
  oled.print("F ");
  drawSignedDelta(petResultFoodDelta);
  oled.print(" H ");
  drawSignedDelta(petResultHungerDelta);

  oled.setCursor(24, 54);
  oled.print("E ");
  drawSignedDelta(petResultEnergyDelta);
  oled.setCursor(75, 54);
  oled.print("SELECT");
  oled.setDrawColor(1);
}

void renderPetSheet() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);

  if (petSheetPage >= PET_SHEET_PAGE_COUNT) {
    petSheetPage = 0;
  }

  const String pageLabel = String(petSheetPage + 1) + "/" + String(PET_SHEET_PAGE_COUNT);
  oled.setFont(u8g2_font_5x8_tf);
  const int pageWidth = oled.getStrWidth(pageLabel.c_str());
  oled.setCursor(128 - pageWidth, 7);
  oled.print(pageLabel);

  oled.setFont(u8g2_font_6x12_tf);
  if (petSheetPage == 0) {
    oled.setCursor(0, 10);
    oled.print("BAT COMBAT");
    oled.setCursor(0, 24);
    oled.print("LV ");
    oled.print(pet.level);
    oled.print(" | XP ");
    oled.print(pet.xp);
    oled.print("/");
    oled.print(xpForNextLevel());
    oled.setCursor(0, 38);
    oled.print("HP ");
    oled.print(pet.hp);
    oled.print(" | ATK ");
    oled.print(pet.atk);
    oled.setCursor(0, 52);
    oled.print("DEF ");
    oled.print(pet.def);
    oled.print(" | LCK ");
    oled.print(pet.lck);
    oled.setCursor(0, 64);
    oled.print("ARENA BEST ");
    oled.print(pet.arenaBest);
  } else if (petSheetPage == 1) {
    oled.setCursor(0, 10);
    oled.print("BAT NEEDS");
    oled.setCursor(0, 24);
    oled.print("ENERGY ");
    oled.print(pet.energy);
    oled.print("/100");
    oled.setCursor(0, 38);
    oled.print("HUNGER ");
    oled.print(pet.hunger);
    oled.print("/100");
    oled.setCursor(0, 52);
    oled.print("FOOD ");
    oled.print(pet.food);
    oled.print("/");
    oled.print(PET_MAX_FOOD);
  } else if (petSheetPage == 2) {
    oled.setCursor(0, 10);
    oled.print("GUIDE");
    oled.setCursor(0, 24);
    oled.print("E = ENERGY");
    oled.setCursor(0, 38);
    oled.print("H = HUNGER");
    oled.setCursor(0, 52);
    oled.print("F = FOOD");
    oled.setCursor(0, 64);
    oled.print("MAX E/H 100");
  } else {
    oled.setCursor(0, 10);
    oled.print("BAT INFO");
    oled.setCursor(0, 28);
    oled.print("TITLE ");
    oled.print(petTitleLabel());
    oled.setCursor(0, 46);
    oled.print("SKILL ");
    oled.print(petSkillLabel());
  }

  oled.sendBuffer();
}

void renderDashboardPage() {
  oled.clearBuffer();

  drawHomeInfo(127, 11);

  const PetRenderSprite petSprite = currentPetSprite();
  drawScaledXbm(petSprite.x,
                PET_Y,
                TAMAGOTCHI_BAT_FRAME_WIDTH,
                TAMAGOTCHI_BAT_FRAME_HEIGHT,
                petSprite.bitmap,
                PET_SCALE_NUM,
                PET_SCALE_DEN,
                petSprite.flipX);

  drawActivityPanel(0, 0, petActivityLabel());
  oled.setFont(u8g2_font_6x12_tf);
  oled.setCursor(91, 35);
  oled.print("LV ");
  oled.print(pet.level);
  drawContinuousStatBar(91, 38, 'E', pet.energy, pet.action == PET_STATE_SLEEP && pet.energy < 100);
  drawContinuousStatBar(91, 45, 'H', pet.hunger, pet.action == PET_STATE_EAT);
  drawSegmentStatBar(91, 52, 'F', pet.food * 10);

  if (petMenuOpen) {
    drawPetMenu();
  }

  drawPetResultModal();
  drawArenaResultModal();

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
  drawPageIndicator(networkPageIndex());
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
  oled.print(proxmoxEnabled ? formatAge(proxmox.lastOkMs, proxmox.valid) : "off");
  oled.print("  WX: ");
  oled.print(weatherEnabled ? formatAge(weather.lastOkMs, weather.valid) : "off");

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

  if (arenaResultOpen || petResultOpen) {
    currentPage = PAGE_DASHBOARD;
    renderDashboardPage();
    return;
  }

  if (petSheetOpen) {
    renderPetSheet();
    return;
  }

  if (currentPage >= pageCount()) {
    currentPage = PAGE_DASHBOARD;
  }

  if (currentPage == PAGE_DASHBOARD) {
    renderDashboardPage();
    return;
  }

  if (currentPage == networkPageIndex()) {
    renderNetworkPage();
    return;
  }

  if (!proxmoxEnabled) {
    renderDashboardPage();
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
  loadDashboardSettings();

  attachInterrupt(digitalPinToInterrupt(BUTTON_LEFT_PIN), onNextButtonFall, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_SELECT_PIN), onSelectButtonFall, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_RIGHT_PIN), onPreviousButtonFall, FALLING);

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
  startNetworkTask();

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
  applyPendingNetworkResults();

  updatePetRoutine();
  updateAlertLed();
  handleButtons();
  renderOled();
  delay(PET_RENDER_DELAY_MS);
}
