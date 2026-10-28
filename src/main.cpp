// SkyScope Mini: a standalone ADS-B radar on a 4.3" ESP32-S3 display. A reduced
// SkyScope: live aircraft from adsb.fi, silhouettes, sweep, filters, alerts and
// major-airport runways, set up from its own web page.
// Board: Sunton ESP32-8048S043 (ESP32-S3, 800x480). A Project Moon Pi project.

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>

#include "board.h"
#include "device.h"
#include "hardware.h"
#include "logo.h"
#include "mini.h"
#include "render.h"

#define SKYSCOPE_MINI_VERSION "1.1.1"

namespace {

constexpr char kProduct[] = "SkyScope Mini";
constexpr unsigned long kFrameMs = 100;

LGFX lcd;
LGFX_Sprite frame(&lcd);
LGFX_Sprite base(&lcd);
WiFiManager wm;
WebServer server(80);
char g_mdnsName[] = "skyscope-mini";

// --- one frame ----------------------------------------------------------------------

void drawFrame() {
  char clock[16] = "";
  const time_t now = time(nullptr);
  if (now > 1700000000) {  // set from the network time servers
    struct tm t;
    gmtime_r(&now, &t);
    strftime(clock, sizeof clock, "%H:%M:%S UTC", &t);
  }
  static char host[24];
  snprintf(host, sizeof host, "%s.local", g_mdnsName);
  mini::lock();
  render::draw(mini::snapshot(), millis(), clock, host, device::address().c_str());
  mini::unlock();
  frame.pushSprite(0, 0);
}

// --- touch: tap an aircraft to select it; - / + change the range --------------------

void handleTouch() {
  static bool wasDown = false;
  int32_t x, y;
  const bool down = lcd.getTouch(&x, &y);
  if (down && !wasDown) {
    const scope::Rect tap{static_cast<int16_t>(x), static_cast<int16_t>(y), 1, 1};
    if (render::kMinus.hits(tap) || render::kPlus.hits(tap)) {
      mini::nextRange(render::kPlus.hits(tap) ? 1 : -1);
      mini::saveSettings();
      mini::requestRefresh();
      render::invalidate();
    } else if (x < scope::geo.panelX) {
      mini::lock();
      render::select(x, y);
      mini::unlock();
    }
  }
  wasDown = down;
}

// --- the settings page ---------------------------------------------------------------

String esc(const String& s) {
  String o;
  for (char c : s) o += c == '<' ? "&lt;" : c == '>' ? "&gt;" : c == '&' ? "&amp;" : c == '"' ? "&quot;" : String(c);
  return o;
}

String checkbox(const char* name, const char* label, bool on) {
  return String("<label class=tick><input type=checkbox name=") + name + (on ? " checked" : "") + ">" + label + "</label>";
}

void handleRoot() {
  using mini::settings;
  String p = F("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>SkyScope Mini</title><style>body{margin:0;background:#040a1c;color:#e8ecf2;font:16px system-ui,sans-serif}"
               "main{max-width:620px;margin:0 auto;padding:20px}h1{color:#46ff78;margin:0}header{display:flex;gap:14px;align-items:center}"
               "section{background:#0a1530;border:1px solid #1d2f55;border-radius:12px;padding:14px 16px;margin-top:16px}"
               "h2{color:#ffc800;font-size:.8rem;letter-spacing:.09em;text-transform:uppercase;margin:0 0 8px}"
               "label{display:block;margin-top:10px;font-weight:600}input,select{width:100%;box-sizing:border-box;padding:9px;margin-top:4px;"
               "background:#071028;color:#e8ecf2;border:1px solid #1d2f55;border-radius:8px;font:inherit}"
               ".tick{display:flex;gap:10px;align-items:center;padding:8px 10px;border:1px solid #1d2f55;border-radius:8px;background:#071028}"
               ".tick input{width:auto;margin:0}.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}"
               "button{margin-top:16px;padding:12px 26px;border:0;border-radius:8px;background:#46ff78;color:#040a1c;font-weight:700;font-size:1rem}"
               ".dim{color:#8a9abb}a{color:#46ff78}</style></head><body><main>"
               "<header><img src=/logo.jpg width=72 height=72 alt='Project Moon Pi'><div><h1>SkyScope Mini</h1>"
               "<div class=dim>A Project Moon Pi project</div></div></header><form method=post action=/save>");
  p += F("<section><h2>Location</h2><div class=row><label>Latitude<input name=lat type=number step=any value='");
  p += String(settings.lat, 5);
  p += F("'></label><label>Longitude<input name=lon type=number step=any value='");
  p += String(settings.lon, 5);
  p += F("'></label></div><p class=dim>The centre of the radar. Tip: find an airport's position on a map, or use SkyScope's airport search.</p></section>");
  p += F("<section><h2>Display</h2><div class=row><label>Distance unit<select name=unit>");
  const char* units[] = {"Kilometres", "Statute miles", "Nautical miles"};
  for (int i = 0; i < 3; ++i) p += String("<option value=") + i + (settings.unit == i ? " selected>" : ">") + units[i] + "</option>";
  p += F("</select></label><label>Range (to the outer ring)<select name=range>");
  for (int i = 0; i < mini::rangeCount(); ++i) {
    const uint8_t keep = settings.rangeIndex;
    settings.rangeIndex = i;
    p += String("<option value=") + i + (keep == i ? " selected>" : ">") + String(mini::rangeValue(), 0) + " " + mini::unitSymbol() + "</option>";
    settings.rangeIndex = keep;
  }
  p += F("</select></label></div>");
  p += checkbox("silh", "Aircraft silhouettes (off: simple triangles)", settings.silhouettes);
  p += checkbox("color", "Colour aircraft by kind", settings.colorByKind);
  p += checkbox("sweep", "Radar sweep", settings.sweep);
  p += checkbox("rwys", "Major-airport runways with extended centrelines", settings.runways);
  p += checkbox("ground", "Aircraft on the ground", settings.ground);
  p += checkbox("emerg", "Highlight emergency squawks (7500 / 7600 / 7700)", settings.emergencies);
  p += F("</section><section><h2>Filters</h2><p class=dim>Tick what to show.</p><div class=row>");
  for (int i = 0; i < mini::kKindCount; ++i) {
    p += checkbox((String("k") + i).c_str(), mini::kKindLabels[i], !(settings.hiddenKinds & (1 << i)));
  }
  p += F("</div><div class=row>");
  for (int i = 0; i < mini::kAltCount; ++i) {
    p += checkbox((String("a") + i).c_str(), mini::kAltLabels[i], !(settings.hiddenAlts & (1 << i)));
  }
  p += F("</div></section><button>Save</button></form>");
  p += F("<section><h2>About</h2><p>SkyScope Mini is a standalone version of <a href='https://github.com/ricsne2010/skyscope'>SkyScope</a> "
         "for a 4.3-inch ESP32-S3 display: live aircraft from adsb.fi (up to 150, within 100 km / 50 mi), silhouettes, the sweep, "
         "filters, emergency alerts and major-airport runways.</p><p class=dim>Version " SKYSCOPE_MINI_VERSION " &middot; ");
  p += esc(String(g_mdnsName) + ".local (" + device::address() + ")");
  p += F("</p><p><a href='https://github.com/ricsne2010/skyscope-mini'>github.com/ricsne2010/skyscope-mini</a></p>"
         "<p class=dim>Hold the BOOT button for 3 seconds to set up Wi-Fi again (10 seconds restores the default display wiring). Board wired differently? <a href='/hardware'>Display hardware (pins)</a>. Aircraft data: adsb.fi. Grew out of "
         "ESP32-Plane-Radar by MatixYo (MIT). Not for navigation.</p></section></main></body></html>");
  server.send(200, "text/html", p);
}

void handleSave() {
  using mini::settings;
  const double lat = server.arg("lat").toDouble(), lon = server.arg("lon").toDouble();
  if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180 && server.arg("lat").length()) {
    settings.lat = lat;
    settings.lon = lon;
  }
  const int unit = server.arg("unit").toInt();
  if (unit != settings.unit && unit >= 0 && unit <= 2) {
    // Keep about the same reach when the unit changes.
    const float km = mini::outerKm();
    settings.unit = static_cast<mini::Unit>(unit);
    float best = 1e9;
    for (int i = 0; i < mini::rangeCount(); ++i) {
      settings.rangeIndex = i;
      const float d = std::fabs(mini::outerKm() - km);
      if (d < best) best = d;
    }
    for (int i = 0; i < mini::rangeCount(); ++i) {
      settings.rangeIndex = i;
      if (std::fabs(mini::outerKm() - km) == best) break;
    }
  } else {
    settings.rangeIndex = server.arg("range").toInt() % mini::rangeCount();
  }
  settings.silhouettes = server.hasArg("silh");
  settings.colorByKind = server.hasArg("color");
  settings.sweep = server.hasArg("sweep");
  settings.runways = server.hasArg("rwys");
  settings.ground = server.hasArg("ground");
  settings.emergencies = server.hasArg("emerg");
  settings.hiddenKinds = 0;
  for (int i = 0; i < mini::kKindCount; ++i) if (!server.hasArg((String("k") + i).c_str())) settings.hiddenKinds |= 1 << i;
  settings.hiddenAlts = 0;
  for (int i = 0; i < mini::kAltCount; ++i) if (!server.hasArg((String("a") + i).c_str())) settings.hiddenAlts |= 1 << i;
  mini::saveSettings();
  mini::requestRefresh();
  render::invalidate();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLogo() {
  size_t len;
  const uint8_t* jpg = logo::smallJpg(&len);
  server.sendHeader("Cache-Control", "max-age=86400");
  server.send_P(200, "image/jpeg", reinterpret_cast<const char*>(jpg), len);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  lcd.configure(hardware::load());  // the wiring saved on /hardware, or the default
  lcd.init();
  lcd.setBrightness(255);
  scope::color::init();
  device::showSplash(lcd, kProduct, "Version " SKYSCOPE_MINI_VERSION);
  delay(2500);

  mini::loadSettings();
  device::connectWifi(lcd, wm, "SkyScope-Mini-Setup", kProduct);
  device::startMdns(g_mdnsName);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/logo.jpg", HTTP_GET, handleLogo);
  hardware::addRoutes(server, kProduct);
  server.begin();

  frame.setPsram(true);
  frame.setColorDepth(16);
  base.setPsram(true);
  base.setColorDepth(16);
  if (!frame.createSprite(board::kWidth, board::kHeight) || !base.createSprite(scope::geo.panelX, board::kHeight)) {
    const char* lines[] = {"Not enough memory for the screen buffers.", "This board needs 8 MB of PSRAM."};
    device::showMessage(lcd, "Can't start", lines, 2);
    for (;;) delay(1000);
  }
  render::begin(frame, base);
  mini::beginFeed();
}

void loop() {
  static unsigned long last = 0;
  server.handleClient();
  if (device::pollBootButton(lcd, wm)) return;  // its instructions are on screen
  handleTouch();
  if (millis() - last >= kFrameMs) {
    last = millis();
    drawFrame();
  }
  delay(2);
}
