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

// Connect to Wi-Fi, opening a setup access point (apName, no password) with
// instructions on screen if there are no saved credentials. `params` are extra
// fields shown on the setup page (may be empty). Blocks until connected.
void connectWifi(LGFX& lcd, WiFiManager& wm, const char* apName, const char* product);

// mDNS name (http://<host>.local) and the address to show on screen.
void startMdns(const char* host);
String address();

// BOOT button: call often. A 3-second hold forgets Wi-Fi and restarts into setup.
void pollBootButton(LGFX& lcd, WiFiManager& wm);

}  // namespace device
