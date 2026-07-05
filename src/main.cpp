#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <U8g2lib.h>
#include <WiFi.h>

#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

U8G2_SH1106_128X64_NONAME_F_SW_I2C oled(
    U8G2_R0,
    OLED_SCL_PIN,
    OLED_SDA_PIN,
    U8X8_PIN_NONE);

const uint8_t MAX_GUESTS = 6;
const uint8_t BASE_PAGE_COUNT = 3;

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

const char *FIRMWARE_VERSION = "OLED v14";

enum DashboardPage : uint8_t {
  PAGE_DASHBOARD = 0,
  PAGE_PROXMOX = 1,
  PAGE_NETWORK = 2,
};

uint8_t currentPage = PAGE_DASHBOARD;
bool displayEnabled = true;

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

  if (selectPressed) {
    displayEnabled = !displayEnabled;
    oled.setPowerSave(displayEnabled ? 0 : 1);
    Serial.print("BTN SELECT -> display ");
    Serial.println(displayEnabled ? "on" : "off");
  }

  if (!displayEnabled) {
    return;
  }

  if (leftPressed) {
    nextPage();
    Serial.print("BTN NEXT -> page ");
    Serial.println(static_cast<uint8_t>(currentPage) + 1);
  }

  if (rightPressed) {
    previousPage();
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

void renderDashboardPage() {
  oled.clearBuffer();

  String time = weatherTime(weather.datetime);
  if (time.length() == 0) {
    time = "--:--";
  }

  String date = weatherDateShort(weather.datetime);
  if (date.length() == 0) {
    date = "--/--/----";
  }

  String humidity = "-- %";
  if (!isnan(weather.humidity)) {
    humidity = String(lround(weather.humidity)) + " %";
  }

  drawDateTimeBlock(0, 0, time, date);
  drawRightAlignedTemperature(127, 12, weather.temperature);
  drawRightAlignedText(127, 27, humidity);

  drawDottedFrame(0, 34, 128, 30);
  oled.drawVLine(42, 38, 22);
  oled.drawVLine(85, 38, 22);
  drawMetricCell(0, 34, 42, proxmox.cpu, "CPU");
  drawMetricCell(43, 34, 42, proxmox.ram, "RAM");
  drawMetricCell(86, 34, 42, proxmox.storage, "DISK");

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
  oled.print("WX IP: ");
  if (weather.ip.length() > 0) {
    oled.print(weather.ip);
  } else {
    oled.print("-");
  }

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

  if (WiFi.status() != WL_CONNECTED) {
    renderLoading("WiFi connect", WIFI_SSID);
    return;
  }

  if (currentPage == PAGE_NETWORK) {
    renderNetworkPage();
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

  attachInterrupt(digitalPinToInterrupt(BUTTON_LEFT_PIN), onLeftButtonFall, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_SELECT_PIN), onSelectButtonFall, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_RIGHT_PIN), onRightButtonFall, FALLING);

#ifdef OLED_DIAGNOSTIC_MODE
  oled.begin();
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

  oled.begin();

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

  const uint32_t now = millis();

  if (lastProxmoxPollMs == 0 || now - lastProxmoxPollMs >= PROXMOX_REFRESH_MS) {
    lastProxmoxPollMs = now;
    pollProxmox();
  }

  if (lastWeatherPollMs == 0 || now - lastWeatherPollMs >= WEATHER_REFRESH_MS) {
    lastWeatherPollMs = now;
    pollWeather();
  }

  handleButtons();
  renderOled();
  delay(50);
}
