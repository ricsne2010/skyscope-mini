// Desktop preview: draws one Mini frame from a saved adsb.fi answer with the
// firmware's own drawing code, and writes it as a PPM image. See build.sh.
//
//   preview adsb.json lat lon unit(km|mi|nm) range-index logo.jpg out.ppm [options]
//   options: kinds (colour by kind), tri (triangles)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "logo.h"
#include "mini.h"
#include "render.h"

namespace {
std::vector<uint8_t> g_logo;

std::string slurp(const char* path) {
  std::ifstream f(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), {});
}
}  // namespace

namespace logo {
const uint8_t* splashJpg(size_t* len) { return smallJpg(len); }
const uint8_t* smallJpg(size_t* len) {
  *len = g_logo.size();
  return g_logo.data();
}
}  // namespace logo

int main(int argc, char** argv) {
  if (argc < 8) {
    fprintf(stderr, "usage: %s adsb.json lat lon km|mi|nm range-index logo.jpg out.ppm [kinds] [tri]\n",
            argv[0]);
    return 2;
  }
  mini::settings.lat = atof(argv[2]);
  mini::settings.lon = atof(argv[3]);
  mini::settings.unit = !strcmp(argv[4], "mi") ? mini::kMi : !strcmp(argv[4], "nm") ? mini::kNm : mini::kKm;
  mini::settings.rangeIndex = static_cast<uint8_t>(atoi(argv[5]) % mini::rangeCount());
  const std::string logoBytes = slurp(argv[6]);
  g_logo.assign(logoBytes.begin(), logoBytes.end());
  for (int i = 8; i < argc; ++i) {
    if (!strcmp(argv[i], "kinds")) mini::settings.colorByKind = true;
    if (!strcmp(argv[i], "tri")) mini::settings.silhouettes = false;
  }

  static mini::Snapshot snap;
  const std::string json = slurp(argv[1]);
  if (!mini::parseAdsb(json.data(), json.size(), snap)) return fprintf(stderr, "%s\n", snap.error), 1;
  const unsigned long now = 2000;
  snap.receivedMs = now;
  printf("%d aircraft, range %g %s\n", snap.count, mini::rangeValue(), mini::unitSymbol());

  scope::color::init();
  LGFX_Sprite frame, base;
  frame.setColorDepth(16);
  base.setColorDepth(16);
  if (!frame.createSprite(board::kWidth, board::kHeight) || !base.createSprite(scope::geo.panelX, board::kHeight)) {
    return fprintf(stderr, "no memory\n"), 1;
  }
  render::begin(frame, base);
  render::draw(snap, now, "18:42:07 UTC", "skyscope-mini.local", "192.168.1.51");

  FILE* out = fopen(argv[7], "wb");
  fprintf(out, "P6\n%d %d\n255\n", board::kWidth, board::kHeight);
  for (int y = 0; y < board::kHeight; ++y) {
    for (int x = 0; x < board::kWidth; ++x) {
      const auto c = frame.readPixelRGB(x, y);
      const uint8_t px[3] = {c.R8(), c.G8(), c.B8()};
      fwrite(px, 1, 3, out);
    }
  }
  fclose(out);
  return 0;
}
