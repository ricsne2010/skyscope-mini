#include "scope.h"

#include "platform.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "shapes.h"

namespace scope {

Geometry geo;
constexpr float kDeg = 0.01745329252f;

uint16_t rgb(uint32_t c) { return lgfx::color565((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF); }

namespace color {
uint16_t bg, panel, grid, ringLabel, label, dim, type, alt, track, runway, centerline, runwayLabel,
    airspaceB, airspaceC, sid, star, highlight, alert, watch, select, sweep;
void init() {
  bg = rgb(0x040A1C);
  panel = rgb(0x071028);
  grid = rgb(0x106420);
  ringLabel = rgb(0x28AA46);
  label = rgb(0xFFFFFF);
  dim = rgb(0x788CAA);
  type = rgb(0xFFC800);
  alt = rgb(0x5AC8FF);
  track = rgb(0xFF00FF);
  runway = rgb(0xA5B9CD);
  centerline = rgb(0x466E87);
  runwayLabel = rgb(0xC4CCDA);  // silver: apart from the tags' blue altitudes
  airspaceB = rgb(0x4673D2);
  airspaceC = rgb(0xA55FB9);
  sid = rgb(0x37B4B4);
  star = rgb(0xD29637);
  highlight = rgb(0xFFC800);
  alert = rgb(0xFF3030);
  watch = rgb(0xFFD60A);
  select = rgb(0x00E5FF);
  sweep = rgb(0x46FF78);
}
}  // namespace color

bool Placed::hits(const Rect& x) const {
  for (int i = 0; i < n; ++i) {
    if (r[i].hits(x)) return true;
  }
  return false;
}

// Even-odd scanline fill; handles the concave silhouettes (swept wings).
void fillPolygon(LGFX_Sprite& g, const float* xs, const float* ys, int n, uint16_t c) {
  if (n < 3) return;
  float minY = ys[0], maxY = ys[0];
  for (int i = 1; i < n; ++i) {
    minY = std::min(minY, ys[i]);
    maxY = std::max(maxY, ys[i]);
  }
  float nodes[24];
  for (int y = static_cast<int>(std::ceil(minY)); y <= static_cast<int>(maxY); ++y) {
    const float sy = y + 0.5f;
    int count = 0;
    for (int i = 0, j = n - 1; i < n && count < 24; j = i++) {
      if ((ys[i] < sy && ys[j] >= sy) || (ys[j] < sy && ys[i] >= sy)) {
        nodes[count++] = xs[i] + (sy - ys[i]) / (ys[j] - ys[i]) * (xs[j] - xs[i]);
      }
    }
    std::sort(nodes, nodes + count);
    for (int k = 0; k + 1 < count; k += 2) {
      const int x0 = static_cast<int>(std::lround(nodes[k]));
      const int x1 = static_cast<int>(std::lround(nodes[k + 1]));
      if (x1 >= x0) g.drawFastHLine(x0, y, x1 - x0 + 1, c);
    }
  }
}

bool clipToCircle(float& x0, float& y0, float& x1, float& y1, float cx, float cy, float r) {
  const float dx = x1 - x0, dy = y1 - y0, fx = x0 - cx, fy = y0 - cy;
  const float a = dx * dx + dy * dy;
  if (a < 1e-9f) return fx * fx + fy * fy <= r * r;
  const float b = 2 * (fx * dx + fy * dy), c = fx * fx + fy * fy - r * r;
  const float disc = b * b - 4 * a * c;
  if (disc < 0) return false;
  const float root = std::sqrt(disc);
  const float t0 = std::max(0.0f, (-b - root) / (2 * a));
  const float t1 = std::min(1.0f, (-b + root) / (2 * a));
  if (t0 >= t1) return false;
  const float nx0 = x0 + dx * t0, ny0 = y0 + dy * t0;
  x1 = x0 + dx * t1;
  y1 = y0 + dy * t1;
  x0 = nx0;
  y0 = ny0;
  return true;
}

void line(LGFX_Sprite& g, float x0, float y0, float x1, float y1, uint16_t c, bool dashed, bool clip) {
  if (clip && !clipToCircle(x0, y0, x1, y1, geo.cx, geo.cy, geo.R)) return;
  if (!dashed) {
    g.drawLine(std::lround(x0), std::lround(y0), std::lround(x1), std::lround(y1), c);
    return;
  }
  const float len = std::hypot(x1 - x0, y1 - y0);
  const int dashes = std::max(1, static_cast<int>(len / 8));
  for (int i = 0; i < dashes; ++i) {
    const float a = static_cast<float>(2 * i) / (2 * dashes), b = static_cast<float>(2 * i + 1) / (2 * dashes);
    g.drawLine(std::lround(x0 + (x1 - x0) * a), std::lround(y0 + (y1 - y0) * a),
               std::lround(x0 + (x1 - x0) * b), std::lround(y0 + (y1 - y0) * b), c);
  }
}

void drawGrid(LGFX_Sprite& g, const float* ringFracs, const char* const* ringLabels, int ringCount, Placed& fixed) {
  const int cx = geo.cx, cy = geo.cy, R = geo.R;
  g.fillRect(0, 0, geo.panelX, board::kHeight, color::bg);
  for (int i = 0; i < ringCount; ++i) {
    const int r = static_cast<int>(R * ringFracs[i]);
    g.drawCircle(cx, cy, r, color::grid);
    g.drawCircle(cx, cy, r - 1, color::grid);
  }
  g.drawFastVLine(cx, cy - R, 2 * R, color::grid);
  g.drawFastHLine(cx - R, cy, 2 * R, color::grid);
  for (int deg = 0; deg < 360; deg += 10) {
    const float a = deg * kDeg, inner = R - (deg % 30 == 0 ? 8 : 4);
    g.drawLine(cx + std::sin(a) * inner, cy - std::cos(a) * inner, cx + std::sin(a) * R, cy - std::cos(a) * R,
               color::grid);
  }
  g.fillCircle(cx, cy, 2, color::label);

  g.setFont(&fonts::FreeSansBold12pt7b);
  g.setTextColor(color::label);
  g.setTextDatum(textdatum_t::middle_center);
  const char* cards[] = {"N", "E", "S", "W"};
  for (int i = 0; i < 4; ++i) {
    const float a = i * 90 * kDeg;
    const int x = cx + std::sin(a) * geo.cardinalR, y = cy - std::cos(a) * geo.cardinalR;
    g.drawString(cards[i], x, y);
    fixed.add({static_cast<int16_t>(x - 10), static_cast<int16_t>(y - 11), 20, 22});
  }
  // Each ring's distance where it crosses the south-east diagonal, on a
  // knockout; the diagonal keeps neighbouring rings' labels apart. A label that
  // would still touch the one before (many close rings) is left out.
  g.setFont(&fonts::FreeSansBold9pt7b);
  g.setTextDatum(textdatum_t::middle_center);
  const float diag = std::sqrt(0.5f);
  Rect prev{0, 0, 0, 0};
  for (int i = 0; i < ringCount; ++i) {
    const float r = R * ringFracs[i];
    const int x = cx + static_cast<int>(r * diag), y = cy + static_cast<int>(r * diag);
    const int w = g.textWidth(ringLabels[i]) + 6, h = 16;
    const Rect box{static_cast<int16_t>(x - w / 2), static_cast<int16_t>(y - h / 2), static_cast<int16_t>(w), h};
    if (i > 0 && box.hits(prev)) continue;
    g.fillRect(box.x, box.y, box.w, box.h, color::bg);
    g.setTextColor(color::ringLabel);
    g.drawString(ringLabels[i], x, y + 1);
    fixed.add(box);
    prev = box;
  }
}

float symbolRadius(int kind) {
  if (kind <= 0 || kind >= shapes::kKindCount) return 9;
  return shapes::kShapes[kind].length_px * geo.symbolScale / 2;
}

static void noseOffset(int kind, float noseDeg, float& dx, float& dy) {
  const float r = symbolRadius(kind), a = noseDeg * kDeg;
  dx = std::sin(a) * r;
  dy = -std::cos(a) * r;
}

void drawSymbol(LGFX_Sprite& g, int kind, float x, float y, float noseDeg, uint16_t c) {
  const float a = noseDeg * kDeg, s = std::sin(a), co = std::cos(a);
  if (kind <= 0 || kind >= shapes::kKindCount) {  // classic red triangle
    const float nose = 9, tail = 3.5f, half = 5;
    const float bx = x - s * tail, by = y + co * tail;
    g.fillTriangle(x + s * nose, y - co * nose, bx + co * half, by + s * half, bx - co * half, by - s * half, c);
    return;
  }
  const shapes::Shape& shape = shapes::kShapes[kind];
  const float half = shape.length_px * geo.symbolScale / 2;
  float xs[24], ys[24];
  for (int p = 0; p < shape.count; ++p) {
    const shapes::Poly& poly = shape.polys[p];
    const int n = std::min<int>(poly.n, 24);
    for (int i = 0; i < n; ++i) {
      const float px = poly.pts[i].x, py = poly.pts[i].y;
      xs[i] = x + (px * co - py * s) * half;
      ys[i] = y + (px * s + py * co) * half;
    }
    fillPolygon(g, xs, ys, n, c);
  }
  if (kind == 7) {  // helicopter rotor disc
    g.drawCircle(x + 0.2f * s * half, y - 0.2f * co * half, half * 0.82f, c);
  }
}

void drawSpeedLine(LGFX_Sprite& g, int kind, float x, float y, float noseDeg, float trackDeg, float len, uint16_t c) {
  float nx, ny;
  noseOffset(kind, noseDeg, nx, ny);
  const float t = trackDeg * kDeg;
  line(g, x + nx, y + ny, x + nx + std::sin(t) * len, y + ny - std::cos(t) * len, c);
}

bool drawTag(LGFX_Sprite& g, float x, float y, float symbolR, const char* const lines[3], const uint16_t colors[3],
             Placed& placed, bool force, uint16_t box) {
  g.setFont(&fonts::FreeSansBold9pt7b);
  int count = 0, w = 0;
  const char* text[3];
  uint16_t cols[3];
  for (int i = 0; i < 3; ++i) {
    if (lines[i] && lines[i][0]) {
      text[count] = lines[i];
      cols[count++] = colors[i];
      w = std::max(w, static_cast<int>(g.textWidth(lines[i])));
    }
  }
  if (!count) return false;
  const int lh = 15, h = lh * count, gap = static_cast<int>(symbolR) + 6;
  const Rect right{static_cast<int16_t>(x + gap), static_cast<int16_t>(y - h / 2), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  const Rect left{static_cast<int16_t>(x - gap - w), static_cast<int16_t>(y - h / 2), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  const Rect above{static_cast<int16_t>(x - w / 2), static_cast<int16_t>(y - gap - h), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  const Rect below{static_cast<int16_t>(x - w / 2), static_cast<int16_t>(y + gap), static_cast<int16_t>(w), static_cast<int16_t>(h)};
  const bool westOfCentre = x < geo.cx;
  const Rect order[4] = {westOfCentre ? right : left, westOfCentre ? left : right, above, below};
  for (int k = 0; k < 4 + (force ? 1 : 0); ++k) {
    const Rect& r = order[k < 4 ? k : 0];
    const bool inBounds = r.x >= 0 && r.y >= 0 && r.x + r.w <= geo.panelX && r.y + r.h <= board::kHeight;
    if (k < 4 && (!inBounds || placed.hits(r))) continue;
    placed.add({static_cast<int16_t>(r.x - 2), static_cast<int16_t>(r.y - 1), static_cast<int16_t>(r.w + 4),
                static_cast<int16_t>(r.h + 2)});
    if (box) {
      g.fillRect(r.x - 4, r.y - 2, r.w + 8, r.h + 4, color::bg);
      g.drawRect(r.x - 4, r.y - 2, r.w + 8, r.h + 4, box);
    }
    const bool onLeft = (k < 4 ? r.x == left.x && r.y == left.y : !westOfCentre);
    g.setTextDatum(onLeft ? textdatum_t::top_right : textdatum_t::top_left);
    for (int i = 0; i < count; ++i) {
      g.setTextColor(cols[i]);
      g.drawString(text[i], onLeft ? r.x + r.w : r.x, r.y + i * lh);
    }
    return true;
  }
  return false;
}

bool Sweep::init() {
  const int size = geo.panelX;  // the scope square is 480 x 480
  angle_ = static_cast<uint8_t*>(SKYSCOPE_PSRAM_CALLOC(size * size, 1));
  if (!angle_) return false;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const float dx = x - geo.cx, dy = y - geo.cy;
      uint8_t v = 255;
      if (dx * dx + dy * dy <= static_cast<float>(geo.R - 1) * (geo.R - 1)) {
        float deg = std::atan2(dx, -dy) / kDeg;  // 0 = north, clockwise
        if (deg < 0) deg += 360;
        v = static_cast<uint8_t>(static_cast<int>(deg / 1.5f) % 240);
      }
      angle_[y * size + x] = v;
    }
  }
  return true;
}

void Sweep::apply(LGFX_Sprite& frame, int stride, float headDeg, float trailDeg) {
  if (!angle_) return;
  const int size = geo.panelX;
  const int head = static_cast<int>(headDeg / 1.5f) % 240;
  const int trail = std::max(4, std::min(239, static_cast<int>(trailDeg / 1.5f)));
  uint8_t alpha[240];  // blend strength by steps behind the beam
  for (int d = 0; d < 240; ++d) {
    const float f = d < trail ? 1.0f - static_cast<float>(d) / trail : 0.0f;
    alpha[d] = static_cast<uint8_t>(110 * std::pow(f, 1.8f));
  }
  alpha[0] = 220;  // the beam itself
  const uint16_t sw = color::sweep;
  const int sr = (sw >> 11) & 0x1F, sg = (sw >> 5) & 0x3F, sb = sw & 0x1F;
  uint16_t* buf = static_cast<uint16_t*>(frame.getBuffer());  // byte-swapped RGB565
  for (int y = 0; y < size; ++y) {
    const uint8_t* arow = angle_ + y * size;
    uint16_t* prow = buf + y * stride;
    for (int x = 0; x < size; ++x) {
      const uint8_t a = arow[x];
      if (a == 255) continue;
      const int d = (head - a + 240) % 240;
      const uint8_t k = alpha[d];
      if (!k) continue;
      uint16_t v = prow[x];
      v = static_cast<uint16_t>((v >> 8) | (v << 8));
      int r = (v >> 11) & 0x1F, gg = (v >> 5) & 0x3F, b = v & 0x1F;
      r += ((sr - r) * k) >> 8;
      gg += ((sg - gg) * k) >> 8;
      b += ((sb - b) * k) >> 8;
      v = static_cast<uint16_t>((r << 11) | (gg << 5) | b);
      prow[x] = static_cast<uint16_t>((v >> 8) | (v << 8));
    }
  }
}

}  // namespace scope
