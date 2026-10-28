// SkyScope Mini: settings, the aircraft model and the ADS-B feed.
#pragma once

#include <cstddef>
#include <cstdint>

namespace mini {

// --- settings (saved in flash) ---------------------------------------------------

enum Unit : uint8_t { kKm = 0, kMi = 1, kNm = 2 };

// Kinds of aircraft (as SkyScope): bit positions in Settings::hiddenKinds.
enum Kind : uint8_t { kAirliner, kBizjet, kGa, kHelicopter, kMilitary, kOther, kKindCount };
extern const char* const kKindLabels[kKindCount];
// Altitude blocks: bit positions in Settings::hiddenAlts. The low block
// reaches 10,000 ft above the ground at the centre (as in SkyScope): over
// Denver that's about 15,200 ft MSL.
enum AltBand : uint8_t { kLow, kMid, kHigh, kAltCount };

struct Settings {
  double lat = 52.3676, lon = 4.9041;
  Unit unit = kKm;
  uint8_t rangeIndex = 1;
  uint8_t hiddenKinds = 0, hiddenAlts = 0;
  bool colorByKind = false, sweep = true, runways = true, ground = false, emergencies = true, silhouettes = true;
  bool elevationAuto = true;   // ground elevation: looked up for the centre, else groundFt
  int16_t groundFt = 0;        // entered elevation, ft above sea level
};
extern Settings settings;
void loadSettings();
void saveSettings();

// Ground elevation (ft MSL) at the centre: the entered one, or the one looked
// up (setLookedUpElevation; NAN until known, then sea level is used).
void setLookedUpElevation(float feet);
float groundElevationFt();
bool groundElevationKnown();
int lowBandTop();                                     // ft MSL: ground + 10,000, below FL180
uint8_t altBand(int32_t altFt);                       // kLow / kMid / kHigh
void altLabel(uint8_t band, char* out, size_t n);     // "15,200 ft and below (10,000 ft AGL)"

// Ranges (to the outer ring) per unit, reduced to suit the Mini.
int rangeCount();
float rangeValue();            // in the display unit
float outerKm();
const char* unitSymbol();
float toUnit(float km);
void nextRange(int step);      // +1 / -1, wrapping
// Rings at a round step, as SkyScope: fractions and labels ("5mi").
int rings(float* fracs, char labels[][10], int max);

// --- aircraft ---------------------------------------------------------------------

constexpr int kMaxAircraft = 150;

struct Plane {
  double lat, lon;          // at the fix
  float track, nose, gs;    // degrees, degrees, knots
  int32_t alt;              // feet; INT32_MIN when unknown
  bool ground, emergency;
  int16_t vrate;            // ft/min; INT16_MIN when unknown
  uint8_t symbol;           // 0 triangle, 1-7 silhouettes (shapes.h)
  uint8_t kind;             // Kind
  uint32_t posAgeMs;
  char hex[8], call[9], type[5], reg[10], desc[30], squawk[5];
};

struct Snapshot {
  Plane planes[kMaxAircraft];
  int count = 0;
  unsigned long receivedMs = 0;  // millis() of the fetch (0 = none yet)
  char error[64] = "";
};

// Background fetch from adsb.fi (its own task). Read with lock()/unlock().
void beginFeed();
void requestRefresh();          // after the location or range changes
void lock();
void unlock();
const Snapshot& snapshot();     // only while locked

// Reads adsb.fi's JSON answer into out (planes, count; error on failure).
bool parseAdsb(const char* json, size_t len, Snapshot& out);

// Classification, as SkyScope's symbols.classify / filters.aircraft_class.
uint8_t classifySymbol(const char* category, const char* type);
uint8_t classifyKind(const char* category, const char* type, const char* call, uint32_t dbFlags, uint8_t symbol);
uint16_t kindColor(uint8_t kind);  // RGB565

}  // namespace mini
