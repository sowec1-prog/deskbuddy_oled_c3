#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <time.h>
#include <math.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "config.h"
#include "weather_icons.h"

constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr uint32_t PAGE_INTERVAL_MS = 8000;
constexpr uint32_t WEATHER_INTERVAL_MS = 600000;
constexpr uint32_t WIFI_TIMEOUT_MS = 12000;
constexpr uint32_t IDLE_RETURN_TO_EYES_MS = 20000;
constexpr uint32_t LIGHT_SLEEP_AFTER_MS = 30UL * 60UL * 1000UL;
constexpr uint32_t HOURLY_CLOCK_SHOW_MS = 30000;

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer setupServer(80);
Preferences preferences;

bool oledReady = false;
bool setupMode = false;
bool webServerStarted = false;
bool weatherReady = false;
uint8_t page = 0;
uint32_t lastPageChange = 0;
uint32_t lastWeatherUpdate = 0;
float temperatureC = NAN;
float feelsC = NAN;
uint8_t humidity = 0;
String weatherText = "Bez dat";
struct ForecastDay {
  char day[6] = "...";
  float temperature = NAN;
  String condition;
  bool valid = false;
};
ForecastDay forecast[3];
String city;
String country;
String weatherKey;
String savedSsid;
String savedPassword;

struct Eye {
  float pupilX = 0;
  float pupilY = 0;
  float targetX = 0;
  float targetY = 0;
};
Eye leftEye, rightEye;
uint32_t lastLook = 0;
bool buttonRawState = false;
bool buttonStableState = false;
bool buttonLongHandled = false;
uint32_t buttonRawChangedAt = 0;
uint32_t buttonPressedAt = 0;
uint32_t lastUserActivity = 0;

bool buttonPressed() {
  const int level = digitalRead(BUTTON_PIN);
  return BUTTON_ACTIVE_LOW ? level == LOW : level == HIGH;
}

void showLine(const String &one, const String &two = "") {
  Serial.println(one + (two.length() ? ": " + two : ""));
  if (!oledReady) return;
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 2);
  display.println(one);
  display.drawLine(0, 13, 127, 13, SH110X_WHITE);
  display.setCursor(0, 24);
  display.println(two);
  display.display();
}

void loadSettings() {
  preferences.begin("deskbuddy", true);
  savedSsid = preferences.getString("ssid", WIFI_SSID);
  savedPassword = preferences.getString("pass", WIFI_PASSWORD);
  weatherKey = preferences.getString("weather", OPENWEATHER_API_KEY);
  city = preferences.getString("city", WEATHER_CITY);
  country = preferences.getString("country", WEATHER_COUNTRY);
  preferences.end();
}

void saveSettings(const String &ssid, const String &password, const String &key,
                  const String &newCity, const String &newCountry) {
  preferences.begin("deskbuddy", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  preferences.putString("weather", key);
  preferences.putString("city", newCity);
  preferences.putString("country", newCountry);
  preferences.end();
}

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("\"", "&quot;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  return value;
}

void handleSetupPage() {
  String form = F("<!doctype html><html><meta name=viewport content='width=device-width,initial-scale=1'>"
                  "<style>body{font-family:sans-serif;max-width:440px;margin:24px auto;padding:18px;background:#102027;color:#eef}"
                  "input,button{box-sizing:border-box;width:100%;padding:11px;margin:5px 0 14px;border-radius:6px;border:1px solid #466}"
                  "button{background:#e28b20;color:#fff;border:0;font-weight:bold}</style>"
                  "<h2>DeskBuddy C3 - local setup</h2><p>Setup AP: DeskBuddy-C3-Setup</p>"
                  "<form method=post action=/save><label>Wi-Fi SSID</label><input name=ssid value='");
  form += htmlEscape(savedSsid);
  form += F("'><label>Wi-Fi password</label><input type=password name=pass placeholder='Zadej pro ulozeni'>"
            "<label>OpenWeatherMap API key (volitelne)</label><input type=password name=weather placeholder='Bez klice budou jen oci a hodiny'>"
            "<label>Mesto</label><input name=city value='");
  form += htmlEscape(city);
  form += F("'><label>Zeme</label><input name=country value='");
  form += htmlEscape(country);
  form += F("'><button type=submit>Ulozit a restartovat</button></form></html>");
  setupServer.send(200, "text/html; charset=utf-8", form);
}

void handleSave() {
  if (!setupServer.hasArg("ssid") || setupServer.arg("ssid").isEmpty()) {
    setupServer.send(400, "text/plain", "Wi-Fi SSID je povinne.");
    return;
  }
  const String password = setupServer.arg("pass");
  String key = setupServer.arg("weather");
  if (key.isEmpty()) key = weatherKey;
  saveSettings(setupServer.arg("ssid"), password, key,
               setupServer.arg("city"), setupServer.arg("country"));
  setupServer.send(200, "text/html", "<h2>Ulozeno. Restartuji...</h2>");
  delay(700);
  ESP.restart();
}

void startLocalWebServer() {
  if (webServerStarted) return;
  setupServer.on("/", HTTP_GET, handleSetupPage);
  setupServer.on("/save", HTTP_POST, handleSave);
  setupServer.begin();
  webServerStarted = true;
  Serial.printf("Localni nastaveni: http://%s/\n", WiFi.localIP().toString().c_str());
}

void startSetupPortal(const String &reason) {
  setupMode = true;
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("DeskBuddy-C3-Setup", "deskbuddy");
  startLocalWebServer();
  Serial.printf("SETUP AP: DeskBuddy-C3-Setup | http://%s/\n", WiFi.softAPIP().toString().c_str());
  showLine("Nastaveni Wi-Fi", "AP: DeskBuddy-C3-Setup");
}

bool connectWiFi() {
  if (savedSsid.isEmpty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
  showLine("Wi-Fi", "Pripojuji...");
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_TIMEOUT_MS) delay(100);
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("Wi-Fi OK: %s\n", WiFi.localIP().toString().c_str());
  startLocalWebServer();
  configTzTime(TIMEZONE_RULE, "pool.ntp.org", "time.nist.gov");
  return true;
}

void getWeather() {
  if (WiFi.status() != WL_CONNECTED || weatherKey.isEmpty()) return;
  WiFiClientSecure client;
  client.setInsecure(); // API certificate is checked by TLS transport; no custom CA is stored on this small device.
  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/weather?q=" + city + "," + country +
               "&appid=" + weatherKey + "&units=metric";
  if (!http.begin(client, url)) return;
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("OpenWeather HTTP %d\n", status);
    weatherReady = false;
    http.end();
    return;
  }
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();
  if (error) {
    Serial.println("OpenWeather JSON error");
    return;
  }
  temperatureC = doc["main"]["temp"] | NAN;
  feelsC = doc["main"]["feels_like"] | NAN;
  humidity = doc["main"]["humidity"] | 0;
  weatherText = String((const char *)(doc["weather"][0]["description"] | "Bez popisu"));
  weatherReady = true;
  Serial.printf("Pocasi OK: %.1f C, %u %%\n", temperatureC, humidity);
}

void getForecast() {
  if (WiFi.status() != WL_CONNECTED || weatherKey.isEmpty()) return;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/forecast?q=" + city + "," + country +
               "&appid=" + weatherKey + "&units=metric";
  if (!http.begin(client, url)) return;
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("OpenWeather forecast HTTP %d\n", status);
    http.end();
    return;
  }
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();
  if (error) {
    Serial.println("OpenWeather forecast JSON error");
    return;
  }
  uint8_t found = 0;
  for (JsonVariant item : doc["list"].as<JsonArray>()) {
    const String timestampText = item["dt_txt"] | "";
    if (!timestampText.endsWith("12:00:00")) continue;
    const time_t timestamp = item["dt"] | 0;
    struct tm localTime;
    localtime_r(&timestamp, &localTime);
    strftime(forecast[found].day, sizeof(forecast[found].day), "%a", &localTime);
    forecast[found].temperature = item["main"]["temp"] | NAN;
    forecast[found].condition = String((const char *)(item["weather"][0]["description"] | "Clouds"));
    forecast[found].valid = true;
    if (++found == 3) break;
  }
  Serial.printf("3denni predpoved: %u/3 dni\n", found);
}

const unsigned char *weatherIcon() {
  String condition = weatherText;
  condition.toLowerCase();
  if (condition.indexOf("rain") >= 0 || condition.indexOf("drizzle") >= 0 || condition.indexOf("thunder") >= 0) return bmp_rain;
  if (condition.indexOf("clear") >= 0) return bmp_clear;
  return bmp_clouds;
}

const unsigned char *forecastIcon(String condition) {
  condition.toLowerCase();
  if (condition.indexOf("rain") >= 0 || condition.indexOf("drizzle") >= 0 || condition.indexOf("thunder") >= 0) return mini_rain;
  if (condition.indexOf("clear") >= 0) return mini_sun;
  return mini_cloud;
}

void drawEyesPage() {
  const uint32_t now = millis();
  if (now - lastLook > 1800) {
    lastLook = now;
    leftEye.targetX = rightEye.targetX = random(-7, 8);
    leftEye.targetY = rightEye.targetY = random(-4, 5);
  }
  leftEye.pupilX += (leftEye.targetX - leftEye.pupilX) * 0.12f;
  leftEye.pupilY += (leftEye.targetY - leftEye.pupilY) * 0.12f;
  rightEye.pupilX += (rightEye.targetX - rightEye.pupilX) * 0.12f;
  rightEye.pupilY += (rightEye.targetY - rightEye.pupilY) * 0.12f;
  const bool blink = (now % 4200) < 130;
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(36, 1);
  display.print("OBLUDA");
  const int eyeH = blink ? 3 : 36;
  const int eyeY = blink ? 31 : 16;
  for (int x : {18, 74}) {
    display.fillRoundRect(x, eyeY, 36, eyeH, 9, SH110X_WHITE);
    if (!blink) {
      const int px = x + 18 + (int)leftEye.pupilX - 8;
      const int py = eyeY + 18 + (int)leftEye.pupilY - 8;
      display.fillRoundRect(px, py, 16, 16, 5, SH110X_BLACK);
      display.fillCircle(px + 11, py + 4, 2, SH110X_WHITE);
    }
  }
  display.display();
}

void drawClockPage() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  struct tm now;
  if (!getLocalTime(&now, 20)) {
    display.setTextSize(1); display.setCursor(15, 25); display.print("Cekam na cas NTP"); display.display(); return;
  }
  char timeText[9], dateText[20];
  strftime(timeText, sizeof(timeText), "%H:%M:%S", &now);
  strftime(dateText, sizeof(dateText), "%a %d.%m.%Y", &now);
  display.setTextSize(2); display.setCursor(16, 20); display.print(timeText);
  display.setTextSize(1); display.setCursor(16, 49); display.print(dateText);
  display.display();
}

void drawWeatherPage() {
  display.clearDisplay(); display.setTextColor(SH110X_WHITE); display.setTextSize(1);
  display.setCursor(0, 1); display.print("POCASI: "); display.print(city);
  display.drawLine(0, 11, 127, 11, SH110X_WHITE);
  if (weatherKey.isEmpty()) {
    display.setCursor(0, 24); display.print("V portalu chybi");
    display.setCursor(0, 37); display.print("OpenWeather API klic.");
  } else if (!weatherReady) {
    display.setCursor(0, 28); display.print("Cekam na data / chyba API");
  } else {
    display.drawBitmap(94, 15, weatherIcon(), 32, 32, SH110X_WHITE);
    display.setTextSize(2); display.setCursor(0, 22); display.printf("%.0f C", temperatureC);
    display.setTextSize(1); display.setCursor(0, 40); display.printf("vlhkost %u %% ", humidity);
    display.drawBitmap(82, 39, bmp_tiny_drop, 8, 8, SH110X_WHITE);
    display.setCursor(0, 50); display.printf("pocit %.0f C", feelsC);
  }
  display.display();
}

void drawForecastPage() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 1);
  display.print("3-DENNI PREDPOVED");
  display.drawLine(42, 12, 42, 63, SH110X_WHITE);
  display.drawLine(85, 12, 85, 63, SH110X_WHITE);
  for (uint8_t i = 0; i < 3; ++i) {
    const int x = i * 43;
    const int center = x + 21;
    display.setCursor(center - 9, 16);
    display.print(forecast[i].valid ? forecast[i].day : "...");
    if (forecast[i].valid) {
      display.drawBitmap(center - 8, 27, forecastIcon(forecast[i].condition), 16, 16, SH110X_WHITE);
      char temp[8];
      snprintf(temp, sizeof(temp), "%.0fC", forecast[i].temperature);
      display.setCursor(center - 8, 52);
      display.print(temp);
    } else {
      display.setCursor(center - 8, 39);
      display.print("cekam");
    }
  }
  display.display();
}

void drawStatusPage() {
  display.clearDisplay(); display.setTextColor(SH110X_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.print("ESP32-C3 DESKBUDDY");
  display.drawLine(0, 10, 127, 10, SH110X_WHITE);
  display.setCursor(0, 18); display.print("OLED SH1106: "); display.print(oledReady ? "OK" : "nenalezen");
  display.setCursor(0, 30); display.print("Wi-Fi: "); display.print(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "odpojena");
  display.setCursor(0, 42); display.print("Mesto: "); display.print(city);
  display.setCursor(0, 54); display.print("Portal pouze pri chybe Wi-Fi");
  display.display();
}

uint64_t microsecondsUntilNextFullHour() {
  time_t epoch;
  time(&epoch);
  // NTP may not be ready immediately after Wi-Fi association. Retry later,
  // rather than pretending an arbitrary time is a full hour.
  if (epoch < 1700000000) return 15ULL * 60ULL * 1000000ULL;
  const time_t nextHour = (epoch / 3600 + 1) * 3600;
  return static_cast<uint64_t>(nextHour - epoch) * 1000000ULL;
}

bool showHourlyClockWindow() {
  page = 1;
  const uint32_t started = millis();
  while (millis() - started < HOURLY_CLOCK_SHOW_MS) {
    drawClockPage();
    if (buttonPressed()) {
      while (buttonPressed()) delay(10); // The wake press only wakes the display.
      return true;
    }
    delay(25);
  }
  return false;
}

void enterLightSleep() {
  Serial.println("Light sleep: OLED vypnuty, cekam na tlacitko nebo celou hodinu");
  while (true) {
    display.clearDisplay();
    display.display();
    display.oled_command(SH110X_DISPLAYOFF);

    const gpio_int_type_t wakeLevel = BUTTON_ACTIVE_LOW ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
    gpio_wakeup_enable(static_cast<gpio_num_t>(BUTTON_PIN), wakeLevel);
    esp_sleep_enable_gpio_wakeup();
    esp_sleep_enable_timer_wakeup(microsecondsUntilNextFullHour());
    const esp_err_t result = esp_light_sleep_start();

    display.oled_command(SH110X_DISPLAYON);
    if (result != ESP_OK) {
      Serial.printf("Light sleep chyba: %d\n", result);
      page = 0;
      lastUserActivity = millis();
      return;
    }
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
      while (buttonPressed()) delay(10); // avoid treating the wake press as page navigation
      page = 0;
      lastUserActivity = millis();
      Serial.println("Light sleep: probuzeno tlacitkem");
      return;
    }
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
      Serial.println("Light sleep: cela hodina, zobrazuji cas na 30 sekund");
      if (showHourlyClockWindow()) {
        page = 0;
        lastUserActivity = millis();
        return;
      }
    }
  }
}

void handleButton() {
  const uint32_t now = millis();
  const bool rawPressed = buttonPressed();

  // A mechanical switch bounces for a few milliseconds. Start a timer on
  // every raw change and accept it only after 45 ms of a stable level.
  if (rawPressed != buttonRawState) {
    buttonRawState = rawPressed;
    buttonRawChangedAt = now;
  }
  if (buttonRawState != buttonStableState && now - buttonRawChangedAt >= BUTTON_DEBOUNCE_MS) {
    buttonStableState = buttonRawState;
    if (buttonStableState) {
      buttonPressedAt = now;
      buttonLongHandled = false;
    } else if (!buttonLongHandled) {
      page = (page + 1) % 4;
      lastUserActivity = now;
      Serial.printf("Stranka: %u\n", page);
    }
  }
  if (buttonStableState && !buttonLongHandled && now - buttonPressedAt >= BUTTON_LONG_PRESS_MS) {
    buttonLongHandled = true;
    startSetupPortal("Rucne vyvolano tlacitkem");
  }
}

void render() {
  if (!oledReady || setupMode) return;
  switch (page) {
    case 0: drawEyesPage(); break;
    case 1: drawClockPage(); break;
    case 2: drawWeatherPage(); break;
    case 3: drawForecastPage(); break;
    default: page = 0; drawEyesPage(); break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("DeskBuddy C3 boot");
  Wire.begin(OLED_SDA, OLED_SCL);
  pinMode(BUTTON_PIN, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  buttonRawState = buttonStableState = buttonPressed();
  buttonRawChangedAt = millis();
  oledReady = display.begin(OLED_ADDRESS, true);
  if (oledReady) showLine("DeskBuddy C3", "OLED SH1106 pripraven");
  else Serial.println("OLED SH1106 nenalezen: zkontroluj SDA=GPIO8, SCL=GPIO9, 3V3 a GND");
  randomSeed(esp_random());
  loadSettings();
  if (!connectWiFi()) startSetupPortal("Wi-Fi neni nastavena nebo je nedostupna");
  else { getWeather(); getForecast(); }
  lastUserActivity = millis();
  lastWeatherUpdate = millis();
}

void loop() {
  if (webServerStarted) setupServer.handleClient();
  if (setupMode) { delay(2); return; }
  const uint32_t now = millis();
  handleButton();
  if (setupMode) return;
  if (page != 0 && now - lastUserActivity >= IDLE_RETURN_TO_EYES_MS) {
    page = 0;
    Serial.println("Neaktivita: navrat na oci");
  }
  if (now - lastWeatherUpdate > WEATHER_INTERVAL_MS) { getWeather(); getForecast(); lastWeatherUpdate = now; }
  if (now - lastUserActivity >= LIGHT_SLEEP_AFTER_MS) enterLightSleep();
  render();
  delay(25);
}
