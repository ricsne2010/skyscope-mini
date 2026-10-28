#include "hardware.h"

#include <Arduino.h>
#include <Preferences.h>

#include <cstdio>
#include <cstring>

namespace hardware {
namespace {

constexpr char kNamespace[] = "board";
constexpr char kKey[] = "pins";

// GPIOs the ESP32-S3 N16R8 can give the display: 26-37 belong to the flash and
// PSRAM, 22-25 don't exist, and 0 is the BOOT button this firmware reads.
bool usable(int pin) { return (pin >= 1 && pin <= 21) || (pin >= 38 && pin <= 48); }

String pinField(const char* name, const char* label, int value, bool optional) {
  char buf[200];
  snprintf(buf, sizeof buf,
           "<label>%s<input name=%s type=number min=%d max=48 value='%d'%s></label>", label, name,
           optional ? -1 : 1, value, optional ? " title='-1 if not connected'" : "");
  return buf;
}

String numField(const char* name, const char* label, int value, int lo, int hi) {
  char buf[200];
  snprintf(buf, sizeof buf, "<label>%s<input name=%s type=number min=%d max=%d value='%d'></label>", label, name, lo,
           hi, value);
  return buf;
}

String select(const char* name, const char* label, int value, const char* const opts[][2], int count) {
  String s = String("<label>") + label + "<select name=" + name + ">";
  for (int i = 0; i < count; ++i) {
    s += String("<option value=") + opts[i][0] + (atoi(opts[i][0]) == value ? " selected>" : ">") + opts[i][1] +
         "</option>";
  }
  return s + "</select></label>";
}

String page(const char* product, const board::Pins& p, const char* message, bool error) {
  String h = F("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Display hardware</title><style>body{margin:0;background:#040a1c;color:#e8ecf2;font:16px system-ui,sans-serif}"
               "main{max-width:720px;margin:0 auto;padding:20px}h1{color:#46ff78;margin:0;font-size:1.5rem}"
               "section{background:#0a1530;border:1px solid #1d2f55;border-radius:12px;padding:14px 16px;margin-top:16px}"
               "h2{color:#ffc800;font-size:.8rem;letter-spacing:.09em;text-transform:uppercase;margin:0 0 8px}"
               ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(110px,1fr));gap:8px 10px}"
               ".lines{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:6px;margin-bottom:8px}"
               ".lines input{padding:8px 2px;text-align:center;-moz-appearance:textfield}"
               ".lines input::-webkit-inner-spin-button{display:none}"
               "label{display:block;font-weight:600;font-size:.9rem}input,select{width:100%;box-sizing:border-box;padding:8px;margin-top:3px;"
               "background:#071028;color:#e8ecf2;border:1px solid #1d2f55;border-radius:8px;font:inherit}"
               "button{margin:16px 10px 0 0;padding:12px 22px;border:0;border-radius:8px;font-weight:700;font-size:1rem}"
               ".save{background:#46ff78;color:#040a1c}.reset{background:#2a3a60;color:#e8ecf2}"
               ".msg{padding:10px 14px;border-radius:10px;margin-top:16px}.err{background:#64101088;border:1px solid #ff5a5a}"
               ".ok{background:#0f4a2a;border:1px solid #46ff78}.warn{color:#ffc800}.dim{color:#8a9abb}a{color:#46ff78}</style></head><body><main>"
               "<h1>Display hardware</h1><div class=dim>");
  h += product;
  h += F(" &middot; <a href='/'>back</a></div>");
  if (message && *message) {
    h += String("<div class='msg ") + (error ? "err" : "ok") + "'>" + message + "</div>";
  }
  h += F("<p class=warn>Only change these if the screen is blank, shifted, flickering or in the wrong colours, "
         "and you have your board's pin list from its seller. The web pages keep working whatever is set here; "
         "<b>Restore defaults</b> (or holding BOOT for 10 seconds) puts back the Sunton ESP32-8048S043 wiring.</p>"
         "<form method=post action=/hardware>");
  h += F("<section><h2>Panel data lines (GPIO)</h2><p class=dim>Blue, green and red bits, least significant first. "
         "Red and blue swapped on screen? Swap the B and R rows.</p>");
  const int rowStart[4] = {0, 5, 11, 16};  // B0-B4, G0-G5, R0-R4
  for (int row = 0; row < 3; ++row) {
    h += "<div class=lines>";
    for (int i = rowStart[row]; i < rowStart[row + 1]; ++i) {
      char name[4];
      snprintf(name, sizeof name, "d%d", i);
      h += pinField(name, board::dataName(i), p.data[i], false);
    }
    h += "</div>";
  }
  h += F("</section><section><h2>Panel control (GPIO)</h2><div class=grid>");
  h += pinField("de", "DE / HENABLE", p.henable, false);
  h += pinField("vs", "VSYNC", p.vsync, false);
  h += pinField("hs", "HSYNC", p.hsync, false);
  h += pinField("pc", "PCLK", p.pclk, false);
  h += pinField("bl", "Backlight", p.backlight, true);
  h += F("</div></section><section><h2>Panel timing</h2><p class=dim>From the panel's data sheet. "
         "A picture that's shifted sideways or rolls usually needs different porches.</p><div class=grid>");
  h += numField("mhz", "Pixel clock, MHz", p.pclkMhz, 4, 20);
  h += numField("hf", "H front porch", p.hFront, 0, 255);
  h += numField("hp", "H sync width", p.hPulse, 1, 255);
  h += numField("hb", "H back porch", p.hBack, 0, 255);
  h += numField("vf", "V front porch", p.vFront, 0, 255);
  h += numField("vp", "V sync width", p.vPulse, 1, 255);
  h += numField("vb", "V back porch", p.vBack, 0, 255);
  static const char* const kPol[][2] = {{"0", "Active low"}, {"1", "Active high"}};
  static const char* const kEdge[][2] = {{"0", "Idle low"}, {"1", "Idle high"}};
  h += select("hpol", "HSYNC polarity", p.hsyncPol, kPol, 2);
  h += select("vpol", "VSYNC polarity", p.vsyncPol, kPol, 2);
  h += select("pidle", "PCLK", p.pclkIdleHigh, kEdge, 2);
  h += F("</div></section><section><h2>Touch</h2><div class=grid>");
  static const char* const kTouch[][2] = {{"0", "None"}, {"1", "GT911"}};
  static const char* const kAddr[][2] = {{"20", "0x14"}, {"93", "0x5D"}};
  h += select("touch", "Touch chip", p.touch, kTouch, 2);
  h += pinField("sda", "SDA", p.sda, false);
  h += pinField("scl", "SCL", p.scl, false);
  h += pinField("trst", "Reset", p.touchRst, true);
  h += pinField("tint", "Interrupt", p.touchInt, true);
  h += select("taddr", "Address", p.touchAddr, kAddr, 2);
  h += F("</div><p class=dim>Unused pins: enter -1 (reset and interrupt only).</p></section>"
         "<button class=save name=action value=save>Save and restart</button>"
         "<button class=reset name=action value=defaults>Restore defaults and restart</button></form>"
         "<p class=dim>Saving restarts the display (about 10 seconds), then this page is at the same address.</p>"
         "</main></body></html>");
  return h;
}

void restartSoon(WebServer& server, const char* product, const char* what) {
  String h = F("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
               "<meta http-equiv=refresh content='12;url=/'><title>Restarting</title><style>body{background:#040a1c;color:#e8ecf2;"
               "font:16px system-ui,sans-serif;padding:24px}h1{color:#46ff78}</style></head><body><h1>");
  h += what;
  h += F("</h1><p>");
  h += product;
  h += F(" is restarting with the new wiring. This page reloads in a few seconds.</p></body></html>");
  server.send(200, "text/html", h);
  delay(600);  // let the answer go out
  ESP.restart();
}

}  // namespace

board::Pins load() {
  board::Pins p = board::defaults();
  Preferences prefs;
  if (prefs.begin(kNamespace, true)) {
    board::Pins saved;
    if (prefs.getBytesLength(kKey) == sizeof saved && prefs.getBytes(kKey, &saved, sizeof saved) == sizeof saved &&
        saved.version == board::kPinsVersion) {
      char err[2];
      if (validate(saved, err, sizeof err)) p = saved;
    }
    prefs.end();
  }
  return p;
}

void save(const board::Pins& p) {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putBytes(kKey, &p, sizeof p);
  prefs.end();
}

void clear() {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.remove(kKey);
  prefs.end();
}

bool validate(const board::Pins& p, char* err, size_t n) {
  struct Use {
    const char* name;
    int pin;
    bool optional;
  };
  Use uses[26];
  int count = 0;
  for (int i = 0; i < 16; ++i) uses[count++] = {board::dataName(i), p.data[i], false};
  uses[count++] = {"DE", p.henable, false};
  uses[count++] = {"VSYNC", p.vsync, false};
  uses[count++] = {"HSYNC", p.hsync, false};
  uses[count++] = {"PCLK", p.pclk, false};
  uses[count++] = {"Backlight", p.backlight, true};
  if (p.touch) {
    uses[count++] = {"Touch SDA", p.sda, false};
    uses[count++] = {"Touch SCL", p.scl, false};
    uses[count++] = {"Touch reset", p.touchRst, true};
    uses[count++] = {"Touch interrupt", p.touchInt, true};
  }
  for (int i = 0; i < count; ++i) {
    if (uses[i].optional && uses[i].pin == -1) continue;
    if (!usable(uses[i].pin)) {
      snprintf(err, n, "%s: GPIO %d can't be used. Choose 1-21 or 38-48 (26-37 run the flash and memory; 0 is BOOT).",
               uses[i].name, uses[i].pin);
      return false;
    }
    for (int j = 0; j < i; ++j) {
      if (uses[j].pin == uses[i].pin) {
        snprintf(err, n, "%s and %s are both set to GPIO %d.", uses[j].name, uses[i].name, uses[i].pin);
        return false;
      }
    }
  }
  if (p.pclkMhz < 4 || p.pclkMhz > 20) return snprintf(err, n, "The pixel clock must be 4 to 20 MHz."), false;
  if (!p.hPulse || !p.vPulse) return snprintf(err, n, "Sync widths must be at least 1."), false;
  if (p.touch && p.touchAddr != 0x14 && p.touchAddr != 0x5D) return snprintf(err, n, "Touch address must be 0x14 or 0x5D."), false;
  return true;
}

void addRoutes(WebServer& server, const char* product) {
  server.on("/hardware", HTTP_GET, [&server, product] {
    server.send(200, "text/html", page(product, load(), nullptr, false));
  });
  server.on("/hardware", HTTP_POST, [&server, product] {
    if (server.arg("action") == "defaults") {
      clear();
      restartSoon(server, product, "Default wiring restored");
      return;
    }
    board::Pins p = board::defaults();
    const auto num = [&server](const char* name, int fallback) {
      return server.hasArg(name) && server.arg(name).length() ? static_cast<int>(server.arg(name).toInt()) : fallback;
    };
    for (int i = 0; i < 16; ++i) {
      char name[4];
      snprintf(name, sizeof name, "d%d", i);
      p.data[i] = num(name, -99);
    }
    p.henable = num("de", -99);
    p.vsync = num("vs", -99);
    p.hsync = num("hs", -99);
    p.pclk = num("pc", -99);
    p.backlight = num("bl", -1);
    p.pclkMhz = constrain(num("mhz", 0), 0, 255);
    p.hFront = constrain(num("hf", 0), 0, 255);
    p.hPulse = constrain(num("hp", 0), 0, 255);
    p.hBack = constrain(num("hb", 0), 0, 255);
    p.vFront = constrain(num("vf", 0), 0, 255);
    p.vPulse = constrain(num("vp", 0), 0, 255);
    p.vBack = constrain(num("vb", 0), 0, 255);
    p.hsyncPol = num("hpol", 0) ? 1 : 0;
    p.vsyncPol = num("vpol", 0) ? 1 : 0;
    p.pclkIdleHigh = num("pidle", 1) ? 1 : 0;
    p.touch = num("touch", 1) ? 1 : 0;
    p.sda = num("sda", -99);
    p.scl = num("scl", -99);
    p.touchRst = num("trst", -1);
    p.touchInt = num("tint", -1);
    p.touchAddr = constrain(num("taddr", 0x14), 0, 255);
    char err[160];
    if (!validate(p, err, sizeof err)) {
      server.send(200, "text/html", page(product, p, err, true));  // keep what they typed
      return;
    }
    save(p);
    restartSoon(server, product, "Wiring saved");
  });
}

}  // namespace hardware
