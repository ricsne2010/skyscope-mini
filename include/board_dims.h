// Screen size and the graphics library, without the hardware driver, so the
// drawing code also builds for the desktop preview (tools/preview).
#pragma once

#ifndef LGFX_USE_V1
#define LGFX_USE_V1
#endif
#include <LovyanGFX.hpp>

namespace board {
constexpr int kWidth = 800;
constexpr int kHeight = 480;
constexpr int kBootPin = 0;  // BOOT button, active LOW
}  // namespace board
