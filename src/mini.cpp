// SkyScope Mini: saved settings and the adsb.fi feed (the board-only parts).
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "mini.h"

namespace mini {

// --- settings -----------------------------------------------------------------------

namespace {
Preferences prefs;
}  // namespace

void loadSettings() {
  prefs.begin("mini", true);
  settings.lat = prefs.getDouble("lat", settings.lat);
  settings.lon = prefs.getDouble("lon", settings.lon);
  settings.unit = static_cast<Unit>(prefs.getUChar("unit", settings.unit) % 3);
  settings.rangeIndex = prefs.getUChar("range", settings.rangeIndex);
  settings.hiddenKinds = prefs.getUChar("kinds", 0);
  settings.hiddenAlts = prefs.getUChar("alts", 0);
  settings.colorByKind = prefs.getBool("color", false);
  settings.sweep = prefs.getBool("sweep", true);
  settings.runways = prefs.getBool("rwys", true);
  settings.ground = prefs.getBool("ground", false);
  settings.emergencies = prefs.getBool("emerg", true);
  settings.silhouettes = prefs.getBool("silh", true);
  prefs.end();
  settings.rangeIndex %= rangeCount();
}

void saveSettings() {
  prefs.begin("mini", false);
  prefs.putDouble("lat", settings.lat);
  prefs.putDouble("lon", settings.lon);
  prefs.putUChar("unit", settings.unit);
  prefs.putUChar("range", settings.rangeIndex);
  prefs.putUChar("kinds", settings.hiddenKinds);
  prefs.putUChar("alts", settings.hiddenAlts);
  prefs.putBool("color", settings.colorByKind);
  prefs.putBool("sweep", settings.sweep);
  prefs.putBool("rwys", settings.runways);
  prefs.putBool("ground", settings.ground);
  prefs.putBool("emerg", settings.emergencies);
  prefs.putBool("silh", settings.silhouettes);
  prefs.end();
}

// --- the adsb.fi feed -----------------------------------------------------------------

namespace {
SemaphoreHandle_t s_mutex;
Snapshot* s_front;
Snapshot* s_back;
volatile bool s_refresh = false;
constexpr unsigned long kPollMs = 5000;  // adsb.fi's public limit is 1 request a second

bool fetch(Snapshot& out) {
  const float fetchKm = outerKm() * 1.06f;  // a little past the ring
  char url[160];
  snprintf(url, sizeof url, "https://opendata.adsb.fi/api/v3/lat/%.5f/lon/%.5f/dist/%.1f", settings.lat,
           settings.lon, fetchKm / 1.852f);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url)) {
    strlcpy(out.error, "Can't reach adsb.fi", sizeof out.error);
    return false;
  }
  http.setUserAgent("SkyScope-Mini/1.0 (+https://github.com/ricsne2010/skyscope-mini)");
  const int code = http.GET();
  if (code != 200) {
    snprintf(out.error, sizeof out.error, "adsb.fi answered %d", code);
    http.end();
    return false;
  }
  const String body = http.getString();  // large bodies go to PSRAM
  http.end();

  return parseAdsb(body.c_str(), body.length(), out);
}

void task(void*) {
  for (;;) {
    s_refresh = false;
    if (WiFi.status() == WL_CONNECTED) {
      const bool ok = fetch(*s_back);
      xSemaphoreTake(s_mutex, portMAX_DELAY);
      if (ok) {
        s_back->receivedMs = millis();
        Snapshot* t = s_front;  // swap: the screen reads the fresh copy
        s_front = s_back;
        s_back = t;
      } else {
        strlcpy(s_front->error, s_back->error, sizeof s_front->error);
      }
      xSemaphoreGive(s_mutex);
    }
    for (unsigned long waited = 0; waited < kPollMs && !s_refresh; waited += 100) vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(1100));  // never more than one request a second
  }
}
}  // namespace

void beginFeed() {
  s_mutex = xSemaphoreCreateMutex();
  s_front = new (ps_calloc(1, sizeof(Snapshot))) Snapshot();
  s_back = new (ps_calloc(1, sizeof(Snapshot))) Snapshot();
  xTaskCreatePinnedToCore(task, "adsb", 16384, nullptr, 1, nullptr, 0);
}

void requestRefresh() { s_refresh = true; }
void lock() { xSemaphoreTake(s_mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGive(s_mutex); }
const Snapshot& snapshot() { return *s_front; }

}  // namespace mini
