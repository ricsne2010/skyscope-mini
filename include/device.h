// Shared device plumbing for the SkyScope Mini and Satellite: the Project Moon
// Pi start-up splash, Wi-Fi setup (captive portal), the BOOT button and mDNS.
#pragma once

#include <WiFiManager.h>

#include "board.h"

namespace device {

// Start-up splash: logo, product name, a subtitle (version), on the scope blue.
void showSplash(LGFX& lcd, const char* product, const char* subtitle);

// A simple full-screen message (setup instructions, errors).
void showMessage(LGFX& lcd, const char* title, const char* const lines[], int count);

// Connect to Wi-Fi. With no saved network, opens a setup access point (apName,
// no password) with instructions on screen; its pages include /hardware.
// Blocks until connected.
void connectWifi(LGFX& lcd, WiFiManager& wm, const char* apName, const char* product);

// mDNS name (http://<host>.local) and the address to show on screen.
void startMdns(const char* host);
String address();

// BOOT button: call often. Held 3 s then released: forget Wi-Fi and restart
// into setup. Held 10 s: restore the default display wiring and restart.
// Returns true while it's held past 3 s and showing instructions (don't draw over them).
bool pollBootButton(LGFX& lcd, WiFiManager& wm);

}  // namespace device
