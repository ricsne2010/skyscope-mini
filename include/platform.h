// Small portability shims so the drawing and parsing code also builds for the
// desktop preview (tools/preview).
#pragma once

#if defined(ARDUINO)
#include <Arduino.h>
#define SKYSCOPE_PSRAM_CALLOC(n, size) ps_calloc((n), (size))  // the board's 8 MB PSRAM
#else
#include <cstdlib>
#define SKYSCOPE_PSRAM_CALLOC(n, size) calloc((n), (size))
#endif
