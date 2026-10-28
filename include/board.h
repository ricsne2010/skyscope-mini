// Board definition: Sunton ESP32-8048S043 ("4.3 inch ESP32-S3 800x480").
// ESP32-S3 (N16R8: 16 MB flash, 8 MB PSRAM), 16-bit RGB panel, GT911 touch,
// backlight on GPIO 2, BOOT button on GPIO 0.
//
// The wiring comes from board_pins.h: defaults() for this board, or what was
// saved on the /hardware web page (hardware::load()). Boards sold under this
// name have had small revisions; that page lets you match yours.
#pragma once

#include "board_dims.h"
#include "board_pins.h"

#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_RGB _bus;
  lgfx::Panel_RGB _panel;
  lgfx::Light_PWM _light;
  lgfx::Touch_GT911 _touch;

 public:
  LGFX() { configure(board::defaults()); }

  // Apply a wiring. Call before init(); the hardware is set up there.
  void configure(const board::Pins& p) {
    {
      auto cfg = _panel.config();
      cfg.memory_width = board::kWidth;
      cfg.memory_height = board::kHeight;
      cfg.panel_width = board::kWidth;
      cfg.panel_height = board::kHeight;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      _panel.config(cfg);
    }
    {
      auto cfg = _panel.config_detail();
      cfg.use_psram = 1;  // the panel's frame buffer lives in PSRAM
      _panel.config_detail(cfg);
    }
    {
      auto cfg = _bus.config();
      cfg.panel = &_panel;
      int8_t* const data[16] = {&cfg.pin_d0, &cfg.pin_d1, &cfg.pin_d2, &cfg.pin_d3, &cfg.pin_d4, &cfg.pin_d5,
                                &cfg.pin_d6, &cfg.pin_d7, &cfg.pin_d8, &cfg.pin_d9, &cfg.pin_d10, &cfg.pin_d11,
                                &cfg.pin_d12, &cfg.pin_d13, &cfg.pin_d14, &cfg.pin_d15};
      for (int i = 0; i < 16; ++i) *data[i] = p.data[i];
      cfg.pin_henable = p.henable;
      cfg.pin_vsync = p.vsync;
      cfg.pin_hsync = p.hsync;
      cfg.pin_pclk = p.pclk;
      cfg.freq_write = p.pclkMhz * 1000000UL;
      cfg.hsync_polarity = p.hsyncPol;
      cfg.hsync_front_porch = p.hFront;
      cfg.hsync_pulse_width = p.hPulse;
      cfg.hsync_back_porch = p.hBack;
      cfg.vsync_polarity = p.vsyncPol;
      cfg.vsync_front_porch = p.vFront;
      cfg.vsync_pulse_width = p.vPulse;
      cfg.vsync_back_porch = p.vBack;
      cfg.pclk_idle_high = p.pclkIdleHigh;
      _bus.config(cfg);
    }
    _panel.setBus(&_bus);
    if (p.backlight >= 0) {
      auto cfg = _light.config();
      cfg.pin_bl = p.backlight;
      _light.config(cfg);
      _panel.light(&_light);
    } else {
      _panel.light(nullptr);
    }
    if (p.touch) {
      auto cfg = _touch.config();
      cfg.x_min = 0;
      cfg.x_max = board::kWidth - 1;
      cfg.y_min = 0;
      cfg.y_max = board::kHeight - 1;
      cfg.pin_int = p.touchInt;
      cfg.pin_rst = p.touchRst;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.i2c_port = 1;
      cfg.pin_sda = p.sda;
      cfg.pin_scl = p.scl;
      cfg.freq = 400000;
      cfg.i2c_addr = p.touchAddr;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    } else {
      _panel.setTouch(nullptr);
    }
    setPanel(&_panel);
  }
};
