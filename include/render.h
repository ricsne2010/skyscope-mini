// Composing the Mini's screen: the scope (static layer with runways, sweep,
// aircraft, labels) and the panel (card, list, range buttons). No Arduino calls
// here, so the desktop preview (tools/preview) draws exactly what the board draws.
#pragma once

#include "mini.h"
#include "scope.h"

namespace render {

// The two on-screen range buttons, in the panel's bottom corner.
extern const scope::Rect kMinus, kPlus;

// frame: the whole 800x480 screen; base: a 480x480 cache of the static scope.
void begin(LGFX_Sprite& frame, LGFX_Sprite& base);
// Redraw the static layer next frame (the location, range or map settings changed).
void invalidate();
// now: milliseconds on the clock that stamped snap.receivedMs. clock: the UTC
// time to show ("" if unknown). host, address: this display's, for the footer.
void draw(const mini::Snapshot& snap, unsigned long now, const char* clock, const char* host, const char* address);
// A tap on the scope: select the aircraft under it, or clear the selection.
// Uses the last frame's positions (call under the same lock as draw).
void select(int x, int y);

}  // namespace render
