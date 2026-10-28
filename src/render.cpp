#include "render.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "data/large_airports.h"
#include "logo.h"

namespace render {

const scope::Rect kMinus{690, 432, 46, 40};
const scope::Rect kPlus{742, 432, 46, 40};

namespace {

constexpr float kKmPerDeg = 111.0f;
constexpr float kDeg = 0.01745329252f;

LGFX_Sprite* s_frame;
LGFX_Sprite* s_base;
scope::Sweep sweep;
scope::Placed fixedLabels;
bool g_rebuild = true;   // the static layer needs redrawing
char g_selected[8] = "";  // hex of the selected aircraft

// --- geometry ---------------------------------------------------------------------

void offsetKm(double lat, double lon, float& dx, float& dy) {
  dx = static_cast<float>(lon - mini::settings.lon) * kKmPerDeg * std::cos(mini::settings.lat * kDeg);
  dy = static_cast<float>(lat - mini::settings.lat) * kKmPerDeg;
}

void toScreen(double lat, double lon, float& x, float& y) {
  float dx, dy;
  offsetKm(lat, lon, dx, dy);
  const float k = scope::geo.R / mini::outerKm();
  x = scope::geo.cx + dx * k;
  y = scope::geo.cy - dy * k;
}

// --- the static layer: grid, rings, major-airport runways ------------------------

void buildBase() {
  LGFX_Sprite& base = *s_base;
  float fracs[6];
  char labels[6][10];
  const char* labelPtrs[6];
  const int n = mini::rings(fracs, labels, 6);
  for (int i = 0; i < n; ++i) labelPtrs[i] = labels[i];
  fixedLabels.clear();
  scope::drawGrid(base, fracs, labelPtrs, n, fixedLabels);
  if (!mini::settings.runways) return;

  using namespace data::large_airports;
  const float reachKm = mini::outerKm() + 25;
  const float pxPerNm = scope::geo.R / mini::outerKm() * 1.852f;
  static bool near[kAirportCount];
  for (size_t i = 0; i < kAirportCount; ++i) {
    float dx, dy;
    offsetKm(kAirports[i].lat_e7 * 1e-7, kAirports[i].lon_e7 * 1e-7, dx, dy);
    near[i] = std::hypot(dx, dy) <= reachKm;
  }
  for (size_t i = 0; i < kRunwayCount; ++i) {
    const Runway& r = kRunways[i];
    if (!near[r.airport_idx]) continue;
    float x0, y0, x1, y1;
    toScreen(r.le_lat_e7 * 1e-7, r.le_lon_e7 * 1e-7, x0, y0);
    toScreen(r.he_lat_e7 * 1e-7, r.he_lon_e7 * 1e-7, x1, y1);
    const float len = std::hypot(x1 - x0, y1 - y0);
    if (r.length_m >= 1524 && len > 0.5f) {  // ATC-style extended centrelines, 10 nm with mile ticks
      const float ux = (x1 - x0) / len, uy = (y1 - y0) / len;
      for (int end = 0; end < 2; ++end) {
        const float ex = end ? x1 : x0, ey = end ? y1 : y0, sx = end ? ux : -ux, sy = end ? uy : -uy;
        scope::line(base, ex, ey, ex + sx * pxPerNm * 10, ey + sy * pxPerNm * 10, scope::color::centerline);
        if (pxPerNm >= 6) {
          for (int k = 1; k <= 10; ++k) {
            const float px = ex + sx * pxPerNm * k, py = ey + sy * pxPerNm * k, h = k % 5 ? 3 : 6;
            scope::line(base, px - uy * h, py + ux * h, px + uy * h, py - ux * h, scope::color::centerline);
          }
        }
      }
    }
    scope::line(base, x0, y0, x1, y1, scope::color::runway);
    scope::line(base, x0 + 1, y0, x1 + 1, y1, scope::color::runway);
  }
  base.setFont(&fonts::Font2);
  base.setTextDatum(textdatum_t::middle_center);
  base.setTextColor(scope::color::runwayLabel);
  for (size_t i = 0; i < kAirportCount; ++i) {
    if (!near[i]) continue;
    float x, y;
    toScreen(kAirports[i].lat_e7 * 1e-7, kAirports[i].lon_e7 * 1e-7, x, y);
    const float dx = x - scope::geo.cx, dy = y - scope::geo.cy, d = std::hypot(dx, dy);
    if (d < 12 || d > scope::geo.R - 12) continue;
    x += dx / d * 16;
    y += dy / d * 16;
    const scope::Rect box{static_cast<int16_t>(x - 18), static_cast<int16_t>(y - 7), 36, 14};
    if (fixedLabels.hits(box)) continue;
    base.drawString(kAirports[i].ident, x, y);
  }
}

// --- one frame ----------------------------------------------------------------------

struct Shown {
  const mini::Plane* p;
  float x, y, dist, bearing, lat, lon;
};
Shown g_shown[mini::kMaxAircraft];
int g_shownCount = 0;

bool passesFilters(const mini::Plane& p) {
  if (p.emergency && mini::settings.emergencies) return true;  // always shown
  if (p.ground && !mini::settings.ground) return false;
  if (mini::settings.hiddenKinds & (1 << p.kind)) return false;
  if (mini::settings.hiddenAlts) {
    if (p.ground) return !(mini::settings.hiddenAlts & 1);
    if (p.alt == INT32_MIN) return false;
    const int band = p.alt <= 10000 ? mini::kLow : p.alt < 18000 ? mini::kMid : mini::kHigh;
    if (mini::settings.hiddenAlts & (1 << band)) return false;
  }
  return true;
}

void formatAlt(const mini::Plane& p, char* out, size_t n, bool unit) {
  if (p.ground) snprintf(out, n, "GND");
  else if (p.alt == INT32_MIN) out[0] = 0;
  else {
    const int a = p.alt;
    if (a >= 1000) snprintf(out, n, "%d,%03d%s", a / 1000, abs(a % 1000), unit ? " ft" : "");
    else snprintf(out, n, "%d%s", a, unit ? " ft" : "");
  }
}

void drawPanel(const mini::Snapshot& snap, const Shown* card, unsigned long now, const char* clock,
               const char* host, const char* address) {
  LGFX_Sprite& frame = *s_frame;
  const int x0 = scope::geo.panelX;
  frame.fillRect(x0, 0, board::kWidth - x0, board::kHeight, scope::color::panel);
  frame.setClipRect(x0, 0, board::kWidth - x0 - 6, board::kHeight);  // long names stop short of the edge
  frame.drawFastVLine(x0, 0, board::kHeight, scope::color::grid);
  size_t len;
  const uint8_t* jpg = logo::smallJpg(&len);
  frame.drawJpg(jpg, len, x0 + 12, 10, 48, 48, 0, 0, 0.5f);
  frame.setTextDatum(textdatum_t::top_left);
  frame.setFont(&fonts::FreeSansBold12pt7b);
  frame.setTextColor(scope::color::sweep);
  frame.drawString("SKYSCOPE", x0 + 68, 12);
  frame.setFont(&fonts::Font2);
  frame.setTextColor(scope::color::dim);
  frame.drawString("MINI", x0 + 68, 40);
  if (clock[0]) {
    frame.setTextDatum(textdatum_t::top_right);
    frame.setTextColor(scope::color::label);
    frame.drawString(clock, board::kWidth - 10, 16);
  }
  int y = 66;
  char line[96];
  frame.setTextDatum(textdatum_t::top_left);
  frame.setTextColor(scope::color::dim);
  snprintf(line, sizeof line, "%.4f, %.4f", mini::settings.lat, mini::settings.lon);
  frame.drawString(line, x0 + 12, y);
  y += 18;
  if (!snap.receivedMs) {
    frame.setTextColor(snap.error[0] ? scope::color::alert : scope::color::dim);
    frame.drawString(snap.error[0] ? snap.error : "Waiting for data...", x0 + 12, y);
  } else {
    const bool filtered = mini::settings.hiddenKinds || mini::settings.hiddenAlts;
    snprintf(line, sizeof line, "%d aircraft%s  -  adsb.fi  -  %lus ago", g_shownCount, filtered ? " (filtered)" : "",
             (now - snap.receivedMs) / 1000);
    frame.setTextColor(snap.error[0] ? scope::color::alert : scope::color::label);
    frame.drawString(line, x0 + 12, y);
  }
  y += 22;
  frame.drawFastHLine(x0 + 12, y, board::kWidth - x0 - 24, scope::color::grid);
  y += 8;

  if (card) {
    const mini::Plane& p = *card->p;
    frame.setTextColor(g_selected[0] ? scope::color::select : scope::color::highlight);
    snprintf(line, sizeof line, "%s - %s", g_selected[0] ? "SELECTED" : "NEAREST", mini::kKindLabels[p.kind]);
    frame.drawString(line, x0 + 12, y);
    y += 16;
    frame.setFont(&fonts::FreeSansBold18pt7b);
    frame.setTextColor(scope::color::label);
    frame.drawString(p.call, x0 + 12, y);
    y += 34;
    frame.setFont(&fonts::Font2);
    frame.setTextColor(scope::color::type);
    // The description without its bracketed variant ("EMBRAER ERJ-170-200 (long range)"), then the registration.
    const char* desc = p.desc[0] ? p.desc : (p.type[0] ? p.type : "Unknown type");
    const char* paren = strstr(desc, " (");
    const int descLen = std::min(paren ? static_cast<int>(paren - desc) : 26, 26);
    if (p.reg[0] && strcmp(p.reg, p.call)) snprintf(line, sizeof line, "%.*s - %s", descLen, desc, p.reg);
    else snprintf(line, sizeof line, "%.*s", descLen, desc);
    frame.drawString(line, x0 + 12, y);
    y += 17;
    char alt[16];
    formatAlt(p, alt, sizeof alt, true);
    if (p.vrate != INT16_MIN && abs(p.vrate) >= 200 && !p.ground) {
      snprintf(line, sizeof line, "%s  (%s %d ft/min)", alt, p.vrate > 0 ? "climbing" : "descending", abs(p.vrate));
    } else {
      snprintf(line, sizeof line, "%s", alt[0] ? alt : "-");
    }
    frame.setTextColor(scope::color::alt);
    frame.drawString(line, x0 + 12, y);
    y += 17;
    snprintf(line, sizeof line, "%.1f %s - brg %03.0f - trk %03.0f - GS %.0f kt", mini::toUnit(card->dist),
             mini::unitSymbol(), card->bearing, p.track, p.gs);
    frame.setTextColor(scope::color::label);
    frame.drawString(line, x0 + 12, y);
    y += 22;
    frame.drawFastHLine(x0 + 12, y, board::kWidth - x0 - 24, scope::color::grid);
    y += 8;
  }
  // The list, nearest first (emergencies on top).
  const int cx[4] = {x0 + 12, x0 + 100, x0 + 214, x0 + 290};
  frame.setTextColor(scope::color::dim);
  const char* head[4] = {"CALLSIGN", "TYPE", "ALT ft", ""};
  char distHead[12];
  snprintf(distHead, sizeof distHead, "DIST %s", mini::unitSymbol());
  head[3] = distHead;
  for (int i = 0; i < 4; ++i) {
    frame.setTextDatum(i < 2 ? textdatum_t::top_left : textdatum_t::top_right);
    frame.drawString(head[i], cx[i], y);
  }
  y += 18;
  for (int i = 0; i < g_shownCount && y < board::kHeight - 50; ++i, y += 17) {
    const Shown& s = g_shown[i];
    char alt[16], dist[12];
    formatAlt(*s.p, alt, sizeof alt, false);
    snprintf(dist, sizeof dist, "%.1f", mini::toUnit(s.dist));
    const char* cells[4] = {s.p->call, s.p->type, alt, dist};
    const bool sel = g_selected[0] && !strcmp(g_selected, s.p->hex);
    const uint16_t first = s.p->emergency ? scope::color::alert : sel ? scope::color::select : scope::color::label;
    const uint16_t cols[4] = {first, scope::color::type, scope::color::alt, scope::color::label};
    for (int k = 0; k < 4; ++k) {
      frame.setTextDatum(k < 2 ? textdatum_t::top_left : textdatum_t::top_right);
      frame.setTextColor(cols[k]);
      frame.drawString(cells[k], cx[k], y);
    }
  }
  // Footer: address, and the range buttons.
  frame.setTextDatum(textdatum_t::bottom_left);
  frame.setTextColor(scope::color::dim);
  frame.drawString(host, x0 + 12, board::kHeight - 26);
  frame.drawString(address, x0 + 12, board::kHeight - 8);
  frame.clearClipRect();
  snprintf(line, sizeof line, "%g %s", mini::rangeValue(), mini::unitSymbol());
  frame.setTextDatum(textdatum_t::middle_right);
  frame.setFont(&fonts::FreeSansBold9pt7b);
  frame.setTextColor(scope::color::sweep);
  frame.drawString(line, kMinus.x - 8, kMinus.y + kMinus.h / 2);
  for (const scope::Rect* b : {&kMinus, &kPlus}) {
    frame.fillRoundRect(b->x, b->y, b->w, b->h, 8, scope::color::grid);
    frame.setFont(&fonts::FreeSansBold18pt7b);
    frame.setTextDatum(textdatum_t::middle_center);
    frame.setTextColor(scope::color::label);
    frame.drawString(b == &kMinus ? "-" : "+", b->x + b->w / 2, b->y + b->h / 2 - 1);
  }
}

}  // namespace

void begin(LGFX_Sprite& frame, LGFX_Sprite& base) {
  s_frame = &frame;
  s_base = &base;
  sweep.init();
}

void invalidate() { g_rebuild = true; }

void draw(const mini::Snapshot& snap, unsigned long now, const char* clock, const char* host, const char* address) {
  LGFX_Sprite& frame = *s_frame;
  if (g_rebuild) {
    g_rebuild = false;
    buildBase();
  }
  s_base->pushSprite(&frame, 0, 0);
  if (mini::settings.sweep) sweep.apply(frame, board::kWidth, std::fmod(now / 4000.0f * 360.0f, 360.0f), 90);

  const float outer = mini::outerKm();
  g_shownCount = 0;
  for (int i = 0; i < snap.count; ++i) {
    const mini::Plane& p = snap.planes[i];
    if (!passesFilters(p)) continue;
    // Dead-reckon along the track since the fix, for smooth motion.
    double lat = p.lat, lon = p.lon;
    if (snap.receivedMs && p.gs > 0) {
      const float km = p.gs * 1.852f * ((now - snap.receivedMs) + p.posAgeMs) / 3600000.0f;
      lat += km * std::cos(p.track * kDeg) / kKmPerDeg;
      lon += km * std::sin(p.track * kDeg) / (kKmPerDeg * std::cos(p.lat * kDeg));
    }
    float dx, dy;
    offsetKm(lat, lon, dx, dy);
    const float dist = std::hypot(dx, dy);
    if (dist > outer && !p.emergency) continue;  // the list and scope show the same set
    Shown& s = g_shown[g_shownCount++];
    s.p = &p;
    s.dist = dist;
    s.bearing = std::fmod(std::atan2(dx, dy) / kDeg + 360.0f, 360.0f);
    toScreen(lat, lon, s.x, s.y);
  }
  std::sort(g_shown, g_shown + g_shownCount, [](const Shown& a, const Shown& b) {
    if (a.p->emergency != b.p->emergency) return a.p->emergency;
    return a.dist < b.dist;
  });

  const bool flashOn = (now / 500) % 2;
  const Shown* card = nullptr;
  for (int i = 0; i < g_shownCount; ++i) {
    if (g_selected[0] && !strcmp(g_selected, g_shown[i].p->hex)) card = &g_shown[i];
  }
  if (!card) {
    g_selected[0] = 0;
    for (int i = 0; i < g_shownCount; ++i) {
      if (!card || g_shown[i].dist < card->dist) card = &g_shown[i];
    }
  }
  for (int i = g_shownCount - 1; i >= 0; --i) {  // far first: nearer symbols on top
    const Shown& s = g_shown[i];
    if (s.dist > outer) continue;
    const mini::Plane& p = *s.p;
    const uint8_t sym = mini::settings.silhouettes ? p.symbol : 0;
    const uint16_t c = p.emergency ? (flashOn ? scope::color::label : scope::color::alert)
                                   : (mini::settings.colorByKind ? mini::kindColor(p.kind) : scope::rgb(0xFF0000));
    const float lenPx = p.gs * 1.852f / 60.0f * scope::geo.R / outer;  // one minute of travel
    scope::drawSpeedLine(frame, sym, s.x, s.y, p.nose, p.track, std::min(40.0f, std::max(5.0f, lenPx)),
                         p.emergency ? scope::color::alert : scope::color::track);
    scope::drawSymbol(frame, sym, s.x, s.y, p.nose, c);
    const float r = scope::symbolRadius(sym);
    if (p.emergency) frame.drawCircle(s.x, s.y, r + 6 + (now / 70) % 10, scope::color::alert);
    if (&s == card) {
      frame.drawCircle(s.x, s.y, r + 5, g_selected[0] ? scope::color::select : scope::color::highlight);
    }
  }
  static scope::Placed placed;
  placed = fixedLabels;
  for (int i = 0; i < g_shownCount; ++i) {
    const float r = scope::symbolRadius(g_shown[i].p->symbol);
    placed.add({static_cast<int16_t>(g_shown[i].x - r), static_cast<int16_t>(g_shown[i].y - r),
                static_cast<int16_t>(2 * r), static_cast<int16_t>(2 * r)});
  }
  for (int i = -1; i < g_shownCount; ++i) {  // the card's aircraft first, then nearest first
    const Shown* s = i < 0 ? card : &g_shown[i];
    if (!s || (i >= 0 && s == card) || s->dist > outer) continue;
    const mini::Plane& p = *s->p;
    char alt[16];
    formatAlt(p, alt, sizeof alt, true);
    const char* lines[3] = {p.emergency ? "EMERGENCY" : p.call, p.emergency ? p.call : p.type, alt};
    const uint16_t cols[3] = {p.emergency ? scope::color::alert : scope::color::label,
                              p.emergency ? scope::color::label : scope::color::type, scope::color::alt};
    scope::drawTag(frame, s->x, s->y, scope::symbolRadius(p.symbol), lines, cols, placed, s == card,
                   s == card && g_selected[0] ? scope::color::select : 0);
  }
  drawPanel(snap, card, now, clock, host, address);
}

void select(int x, int y) {
  const Shown* best = nullptr;
  float bestD = 24;
  for (int i = 0; i < g_shownCount; ++i) {
    const float d = std::hypot(g_shown[i].x - x, g_shown[i].y - y);
    if (d < bestD) best = &g_shown[i], bestD = d;
  }
  if (best && strcmp(g_selected, best->p->hex)) snprintf(g_selected, sizeof g_selected, "%s", best->p->hex);
  else g_selected[0] = 0;  // tapping the selection or empty space clears it
}

}  // namespace render
