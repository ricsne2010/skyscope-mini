// The display's wiring, as a value that can be saved and changed from the web
// page (/hardware) without rebuilding the firmware. defaults() is the Sunton
// ESP32-8048S043 as its vendor documents it.
#pragma once

#include <cstdint>

namespace board {

constexpr uint16_t kPinsVersion = 1;  // bump when Pins changes shape

struct Pins {
  uint16_t version;
  int8_t data[16];           // panel data lines B0-B4, G0-G5, R0-R4
  int8_t henable, vsync, hsync, pclk;
  uint8_t pclkMhz;           // pixel clock
  uint8_t hFront, hPulse, hBack, vFront, vPulse, vBack;  // porches and sync widths, in pixels / lines
  uint8_t hsyncPol, vsyncPol, pclkIdleHigh;
  int8_t backlight;          // -1: none
  uint8_t touch;             // 0 none, 1 GT911 (capacitive)
  int8_t sda, scl, touchRst, touchInt;  // -1: not connected (reset, interrupt)
  uint8_t touchAddr;         // GT911 answers on 0x14 or 0x5D
};

inline Pins defaults() {
  return Pins{kPinsVersion,
              {8, 3, 46, 9, 1, 5, 6, 7, 15, 16, 4, 45, 48, 47, 21, 14},
              40, 41, 39, 42,
              16,
              8, 4, 43, 8, 4, 12,
              0, 0, 1,
              2,
              1, 19, 20, 38, -1, 0x14};
}

// Names of the data lines, for the web page.
inline const char* dataName(int i) {
  static const char* const kNames[16] = {"B0", "B1", "B2", "B3", "B4", "G0", "G1", "G2",
                                         "G3", "G4", "G5", "R0", "R1", "R2", "R3", "R4"};
  return kNames[i];
}

}  // namespace board
