#include "device.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "hardware.h"
#include "logo.h"
#include "scope.h"

extern const uint8_t splash_jpg_start[] asm("_binary_data_splash_jpg_start");
extern const uint8_t splash_jpg_end[] asm("_binary_data_splash_jpg_end");
extern const uint8_t logo_jpg_start[] asm("_binary_data_logo_jpg_start");
extern const uint8_t logo_jpg_end[] asm("_binary_data_logo_jpg_end");

namespace logo {
const uint8_t* splashJpg(size_t* len) {
  *len = splash_jpg_end - splash_jpg_start;
  return splash_jpg_start;
}
const uint8_t* smallJpg(size_t* len) {
  *len = logo_jpg_end - logo_jpg_start;
  return logo_jpg_start;
}
}  // namespace logo

namespace device {

void showSplash(LGFX& lcd, const char* product, const char* subtitle) {
  lcd.fillScreen(scope::color::bg);
  size_t len;
  const uint8_t* jpg = logo::splashJpg(&len);
  lcd.drawJpg(jpg, len, (board::kWidth - 330) / 2, 18);
  lcd.setTextDatum(textdatum_t::top_center);
  lcd.setFont(&fonts::FreeSansBold18pt7b);
  lcd.setTextColor(scope::color::sweep);
  lcd.drawString(product, board::kWidth / 2, 362);
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(scope::color::dim);
  lcd.drawString(subtitle, board::kWidth / 2, 408);
  lcd.drawString("A Project Moon Pi project", board::kWidth / 2, 432);
}

void showMessage(LGFX& lcd, const char* title, const char* const lines[], int count) {
  lcd.fillScreen(scope::color::bg);
  size_t len;
  const uint8_t* jpg = logo::smallJpg(&len);
  lcd.drawJpg(jpg, len, 40, 40);
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setFont(&fonts::FreeSansBold18pt7b);
  lcd.setTextColor(scope::color::sweep);
  lcd.drawString(title, 156, 64);
  lcd.setFont(&fonts::FreeSans12pt7b);
  lcd.setTextColor(scope::color::label);
  for (int i = 0; i < count; ++i) lcd.drawString(lines[i], 40, 170 + i * 36);
}

void connectWifi(LGFX& lcd, WiFiManager& wm, const char* apName, const char* product) {
  WiFi.mode(WIFI_STA);
  static String ap, prod;
  ap = apName;
  prod = product;
  wm.setTitle(product);
  wm.setConfigPortalTimeout(0);  // stay in setup until someone completes it
  wm.setConnectTimeout(20);
  // The display-wiring page is in the setup portal too: with the wrong pins the
  // screen shows nothing, but the portal (hold BOOT 3 s) still works.
  static const char* menu[] = {"wifi", "custom", "info", "restart"};
  wm.setMenu(menu, 4);
  wm.setCustomMenuHTML("<form action='/hardware' method='get'><button>Display hardware</button></form><br/>");
  wm.setWebServerCallback([&wm] { hardware::addRoutes(*wm.server, prod.c_str()); });
  wm.setAPCallback([&lcd](WiFiManager*) {
    const String l1 = "1. On a phone, join the Wi-Fi network  " + ap;
    const char* lines[] = {
        "This display needs your Wi-Fi details.",
        l1.c_str(),
        "2. A setup page opens (or go to http://192.168.4.1)",
        "3. Choose your network, enter its password, Save",
        "Hold BOOT 3 s to start setup again; 10 s restores the default pins.",
    };
    showMessage(lcd, (prod + " setup").c_str(), lines, 5);
  });
  if (!wm.autoConnect(apName)) {
    ESP.restart();
  }
}

void startMdns(const char* host) {
  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);
  }
}

String address() { return WiFi.localIP().toString(); }

bool pollBootButton(LGFX& lcd, WiFiManager& wm) {
  static unsigned long downSince = 0;
  static bool told = false;
  pinMode(board::kBootPin, INPUT_PULLUP);
  const unsigned long held = downSince ? millis() - downSince : 0;
  if (digitalRead(board::kBootPin) == LOW) {
    if (!downSince) downSince = millis();
    if (held > 10000) {
      const char* lines[] = {"Restoring the default display wiring...", "The display restarts."};
      showMessage(lcd, "Display reset", lines, 2);
      hardware::clear();
      delay(1500);
      ESP.restart();
    }
    if (held > 3000 && !told) {
      told = true;
      const char* lines[] = {"Let go now to forget this Wi-Fi network",
                             "and start Wi-Fi setup.",
                             "Keep holding to 10 seconds to restore the",
                             "display's default wiring (pins) instead."};
      showMessage(lcd, "BOOT button", lines, 4);
    }
    return held > 3000;
  }
  if (held > 3000) {  // released between 3 and 10 seconds
    const char* lines[] = {"Forgetting this Wi-Fi network...", "The display restarts into setup."};
    showMessage(lcd, "Wi-Fi reset", lines, 2);
    wm.resetSettings();
    delay(1500);
    ESP.restart();
  }
  downSince = 0;
  told = false;
  return false;
}

}  // namespace device
