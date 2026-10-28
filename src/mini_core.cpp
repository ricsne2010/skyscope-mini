// SkyScope Mini: the parts with no hardware in them (ranges, rings,
// classification, reading adsb.fi's answer), so the desktop preview
// (tools/preview) runs the same code as the board.
#include <ArduinoJson.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "mini.h"
#include "scope.h"
#include "shapes.h"

namespace mini {

const char* const kKindLabels[kKindCount] = {"Airliners", "Business jets", "General aviation",
                                              "Helicopters", "Military", "Other / unknown"};
Settings settings;

// --- altitude blocks, counted from the ground -----------------------------------------

namespace {
float s_lookedUpFt = NAN;

void withCommas(int v, char* out, size_t n) {
  if (v >= 1000) snprintf(out, n, "%d,%03d", v / 1000, v % 1000);
  else snprintf(out, n, "%d", v);
}
}  // namespace

void setLookedUpElevation(float feet) { s_lookedUpFt = feet; }

bool groundElevationKnown() { return !settings.elevationAuto || !std::isnan(s_lookedUpFt); }

float groundElevationFt() {
  if (!settings.elevationAuto) return settings.groundFt;
  return std::isnan(s_lookedUpFt) ? 0.0f : s_lookedUpFt;
}

int lowBandTop() {
  const float top = std::round((std::max(0.0f, groundElevationFt()) + 10000.0f) / 100.0f) * 100.0f;
  return static_cast<int>(std::min(17900.0f, top));
}

uint8_t altBand(int32_t altFt) {
  if (altFt <= lowBandTop()) return kLow;
  return altFt < 18000 ? kMid : kHigh;
}

void altLabel(uint8_t band, char* out, size_t n) {
  const int top = lowBandTop();
  char a[12], b[12];
  withCommas(top, a, sizeof a);
  withCommas(top + 1, b, sizeof b);
  if (band == kHigh) snprintf(out, n, "FL180 to FL600");
  else if (band == kMid) snprintf(out, n, "%s ft to FL180", b);
  else if (top == 10000) snprintf(out, n, "10,000 ft and below");
  else snprintf(out, n, "%s ft and below (10,000 ft AGL)", a);
}

namespace {
constexpr float kRanges[3][6] = {{5, 10, 15, 25, 50, 100}, {2, 5, 10, 15, 25, 50}, {2, 5, 10, 20, 40, 40}};
constexpr int kRangeCounts[3] = {6, 6, 5};
constexpr float kPerKm[3] = {1.0f, 1 / 1.609344f, 1 / 1.852f};
constexpr const char* kUnitSymbols[3] = {"km", "mi", "nm"};

void copyStr(char* dst, size_t n, JsonVariantConst v) {
  const char* s = v.as<const char*>();
  snprintf(dst, n, "%s", s ? s : "");
  size_t len = strlen(dst);
  while (len > 0 && dst[len - 1] == ' ') dst[--len] = 0;  // callsigns are space-padded
}

float firstNumber(JsonObjectConst p, const char* a, const char* b, const char* c) {
  for (const char* k : {a, b, c}) {
    if (k && p[k].is<float>()) return p[k].as<float>();
  }
  return NAN;
}
}  // namespace

int rangeCount() { return kRangeCounts[settings.unit]; }
float rangeValue() { return kRanges[settings.unit][settings.rangeIndex % rangeCount()]; }
float outerKm() { return rangeValue() / kPerKm[settings.unit]; }
const char* unitSymbol() { return kUnitSymbols[settings.unit]; }
float toUnit(float km) { return km * kPerKm[settings.unit]; }

void nextRange(int step) {
  settings.rangeIndex = (settings.rangeIndex + rangeCount() + step) % rangeCount();
}

int rings(float* fracs, char labels[][10], int max) {
  // A round step (1, 2, 2.5 or 5 x 10^k), whole numbers preferred from 4 up.
  const float r = rangeValue();
  int n = 4;
  for (int cand : {4, 5, 3, 2}) {
    const float step = r / cand;
    const float mant = step / std::pow(10.0f, std::floor(std::log10(step)));
    const bool round = std::fabs(mant - 1) < 1e-3f || std::fabs(mant - 2) < 1e-3f ||
                       std::fabs(mant - 2.5f) < 1e-3f || std::fabs(mant - 5) < 1e-3f;
    if (round && (std::fabs(step - std::round(step)) < 1e-3f || r < 4)) {
      n = cand;
      break;
    }
  }
  n = std::min(n, max);
  for (int i = 0; i < n; ++i) {
    fracs[i] = static_cast<float>(i + 1) / n;
    snprintf(labels[i], 10, "%g%s", r * (i + 1) / n, unitSymbol());
  }
  return n;
}

// --- classification -----------------------------------------------------------------

namespace {
bool startsWithAny(const char* s, const char* const* list, int n) {
  for (int i = 0; i < n; ++i) {
    if (strncmp(s, list[i], strlen(list[i])) == 0) return true;
  }
  return false;
}
}  // namespace

uint8_t classifySymbol(const char* cat, const char* type) {
  using namespace shapes;
  char t[8] = "";
  for (int i = 0; type[i] && i < 7; ++i) t[i] = toupper(type[i]), t[i + 1] = 0;
  const auto is = [&](const char* c) { return strcmp(cat, c) == 0; };
  static const char* const kGlider[] = {"GLID", "DG", "ASK", "LS"};
  if (startsWithAny(t, kHeliTypes, kHeliTypesCount) || is("A7")) return 7;
  if (is("B1") || startsWithAny(t, kGlider, 4)) return 6;
  if (startsWithAny(t, kHeavyTypes, kHeavyTypesCount) || is("A5")) return 2;
  if (startsWithAny(t, kTurbopropTypes, kTurbopropTypesCount)) return 4;
  if (startsWithAny(t, kBizjetTypes, kBizjetTypesCount) || is("A6")) return 3;
  if (startsWithAny(t, kLightTypes, kLightTypesCount) || is("A1") || is("B4")) return 5;
  if (startsWithAny(t, kAirlinerTypes, kAirlinerTypesCount) || is("A3") || is("A4")) return 1;
  if (is("A2")) return 3;
  return cat[0] ? 1 : 5;  // no category and an unknown type: most often a light aircraft
}

uint8_t classifyKind(const char* cat, const char* type, const char* call, uint32_t dbFlags, uint8_t symbol) {
  if (!strcmp(type, "TWR") || !strcmp(type, "GND") || !strcmp(type, "GRND") || !strcmp(type, "SERV")) return kOther;
  if (dbFlags & 1) return kMilitary;
  if (symbol == 7) return kHelicopter;
  const bool noCat = !cat[0] || !strcmp(cat, "A0") || !strcmp(cat, "B0") || cat[0] == 'C';
  if (!type[0] && noCat) return kOther;
  if (symbol == 3) return kBizjet;
  if (symbol == 1 || symbol == 2) return kAirliner;
  const bool airlineCall = isalpha(call[0]) && isalpha(call[1]) && isalpha(call[2]) && isdigit(call[3]);
  if (symbol == 4 && airlineCall) return kAirliner;
  return kGa;
}

uint16_t kindColor(uint8_t kind) {
  static const uint32_t kColors[kKindCount] = {0xFF4D4D, 0xFFA53C, 0x50C8FF, 0xD278FF, 0xD7C86E, 0xA0A8B4};
  return scope::rgb(kColors[kind < kKindCount ? kind : kOther]);
}

// --- adsb.fi's answer -------------------------------------------------------------------

bool parseAdsb(const char* json, size_t len, Snapshot& out) {
  JsonDocument filter;
  JsonObject f = filter["ac"].add<JsonObject>();
  for (const char* k : {"hex", "flight", "r", "t", "desc", "category", "lat", "lon", "track", "true_heading",
                        "mag_heading", "gs", "tas", "alt_baro", "alt_geom", "baro_rate", "squawk", "emergency",
                        "dbFlags", "seen_pos"}) {
    f[k] = true;
  }
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, json, len, DeserializationOption::Filter(filter));
  if (err) {
    snprintf(out.error, sizeof out.error, "Bad data from adsb.fi: %s", err.c_str());
    return false;
  }
  out.count = 0;
  for (JsonObjectConst p : doc["ac"].as<JsonArrayConst>()) {
    if (out.count >= kMaxAircraft) break;
    if (!p["lat"].is<float>() || !p["lon"].is<float>()) continue;
    Plane& a = out.planes[out.count];
    a.lat = p["lat"].as<double>();
    a.lon = p["lon"].as<double>();
    a.ground = p["alt_baro"].is<const char*>() && !strcmp(p["alt_baro"].as<const char*>(), "ground");
    const float alt = firstNumber(p, "alt_baro", "alt_geom", nullptr);
    a.alt = a.ground || std::isnan(alt) ? INT32_MIN : static_cast<int32_t>(alt);
    const float track = firstNumber(p, "track", "true_heading", "mag_heading");
    a.track = std::isnan(track) ? 0 : track;
    const float nose = firstNumber(p, "true_heading", "mag_heading", "track");
    a.nose = std::isnan(nose) ? a.track : nose;
    const float gs = firstNumber(p, "gs", "tas", nullptr);
    a.gs = std::isnan(gs) ? 0 : gs;
    const float vr = firstNumber(p, "baro_rate", nullptr, nullptr);
    a.vrate = std::isnan(vr) ? INT16_MIN : static_cast<int16_t>(std::max(-32000.0f, std::min(32000.0f, vr)));
    const float seen = firstNumber(p, "seen_pos", nullptr, nullptr);
    a.posAgeMs = std::isnan(seen) ? 0 : static_cast<uint32_t>(std::min(30.0f, std::max(0.0f, seen)) * 1000);
    copyStr(a.hex, sizeof a.hex, p["hex"]);
    copyStr(a.call, sizeof a.call, p["flight"]);
    if (!a.call[0]) {
      for (int i = 0; a.hex[i] && i < 8; ++i) a.call[i] = toupper(a.hex[i]), a.call[i + 1] = 0;
    }
    copyStr(a.type, sizeof a.type, p["t"]);
    copyStr(a.reg, sizeof a.reg, p["r"]);
    copyStr(a.desc, sizeof a.desc, p["desc"]);
    copyStr(a.squawk, sizeof a.squawk, p["squawk"]);
    char cat[4];
    copyStr(cat, sizeof cat, p["category"]);
    const char* em = p["emergency"].as<const char*>();
    a.emergency = !strcmp(a.squawk, "7500") || !strcmp(a.squawk, "7600") || !strcmp(a.squawk, "7700") ||
                  (em && strcmp(em, "none") != 0);
    a.symbol = classifySymbol(cat, a.type);
    a.kind = classifyKind(cat, a.type, a.call, p["dbFlags"] | 0u, a.symbol);
    ++out.count;
  }
  out.error[0] = 0;
  return true;
}

}  // namespace mini
