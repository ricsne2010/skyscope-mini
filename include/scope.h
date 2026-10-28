// Shared radar-scope drawing for the SkyScope Mini and Satellite.
// Everything draws into LovyanGFX sprites; positions on the scope are given in
// "ring units": x right, y down, the outer range ring at radius 1.0.
#pragma once

#include <cstdint>

#include "board_dims.h"

namespace scope {

// Colours (RGB565), matching SkyScope's palette.
uint16_t rgb(uint32_t rgb888);
namespace color {
extern uint16_t bg, panel, grid, ringLabel, label, dim, type, alt, track, runway, centerline,
    runwayLabel, airspaceB, airspaceC, sid, star, highlight, alert, watch, select, sweep;
void init();
}  // namespace color

// Layout of the 800x480 screen: scope on the left, panel on the right.
struct Geometry {
  int cx = 240, cy = 240;   // scope centre
  int R = 206;              // outer ring radius, px
  int cardinalR = 226;      // N/E/S/W labels
  int dotR = 216;           // dots for aircraft beyond the ring
  int panelX = 480;         // panel's left edge
  float symbolScale = 0.6f; // silhouette size: px per unit of SkyScope's 1080p length
  int toX(float rx) const { return cx + static_cast<int>(rx * R); }
  int toY(float ry) const { return cy + static_cast<int>(ry * R); }
};
extern Geometry geo;

struct Rect {
  int16_t x, y, w, h;
  bool hits(const Rect& o) const { return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h; }
};

// A small fixed list of placed rectangles (labels, symbols) for collision tests.
struct Placed {
  static constexpr int kMax = 400;
  Rect r[kMax];
  int n = 0;
  void clear() { n = 0; }
  void add(const Rect& x) { if (n < kMax) r[n++] = x; }
  bool hits(const Rect& x) const;
};

// Primitives.
void fillPolygon(LGFX_Sprite& g, const float* xs, const float* ys, int n, uint16_t c);
bool clipToCircle(float& x0, float& y0, float& x1, float& y1, float cx, float cy, float r);
void line(LGFX_Sprite& g, float x0, float y0, float x1, float y1, uint16_t c, bool dashed = false, bool clip = true);

// The static scope: background, rings with their distance labels, crosshair,
// bearing ticks, N/E/S/W. rings[i] is the ring's fraction of the outer radius.
// Adds the fixed labels to `fixed` so aircraft labels can avoid them.
void drawGrid(LGFX_Sprite& g, const float* ringFracs, const char* const* ringLabels, int ringCount, Placed& fixed);

// Aircraft. kind: 0 classic triangle, 1-7 silhouettes (see shapes.h).
float symbolRadius(int kind);
void drawSymbol(LGFX_Sprite& g, int kind, float x, float y, float noseDeg, uint16_t c);
// Speed line from the nose along the track, clipped to the ring; len in px.
void drawSpeedLine(LGFX_Sprite& g, int kind, float x, float y, float noseDeg, float trackDeg, float len, uint16_t c);

// Label of up to three lines beside (x, y), avoiding `placed`; tries toward the
// centre, the far side, above, below. Returns false if nothing fitted (unless
// force). Lines may be null/empty. colors[i] for each line.
bool drawTag(LGFX_Sprite& g, float x, float y, float symbolR, const char* const lines[3],
             const uint16_t colors[3], Placed& placed, bool force = false, uint16_t box = 0);

// The sweep: a rotating beam with a fading trail, blended into the scope
// region of `frame` (whose width is `stride` pixels). Call init() once.
class Sweep {
 public:
  bool init();
  void apply(LGFX_Sprite& frame, int stride, float headDeg, float trailDeg);

 private:
  uint8_t* angle_ = nullptr;  // per pixel of the scope square: 0..239 (1.5 deg), 255 = outside
};

}  // namespace scope
