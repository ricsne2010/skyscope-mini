// The display's wiring, saved in flash and changed from the /hardware web page,
// so a board revision with different pins needs no rebuild.
#pragma once

#include <WebServer.h>

#include <cstddef>

#include "board_pins.h"

namespace hardware {

board::Pins load();  // the saved wiring, or the defaults if none (or it doesn't check out)
void save(const board::Pins& p);
void clear();        // back to the defaults

// Checks a wiring; on failure writes why into err and returns false.
bool validate(const board::Pins& p, char* err, size_t n);

// Adds GET/POST /hardware to a web server: the form, Save, Restore defaults.
// Saving restarts the board so the new wiring takes effect. Used on the
// display's own page and in the Wi-Fi setup portal (reachable when the
// screen shows nothing).
void addRoutes(WebServer& server, const char* product);

}  // namespace hardware
