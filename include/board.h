// Board definition: Sunton ESP32-8048S043 ("4.3 inch ESP32-S3 800x480").
// ESP32-S3 (N16R8: 16 MB flash, 8 MB PSRAM), 16-bit RGB panel, GT911 touch,
// backlight on GPIO 2, BOOT button on GPIO 0.
//
// The pin map follows the board vendor's reference configuration. Boards sold
// under this name have had small revisions: if the picture is shifted or the
// colours are wrong, compare these pins with the documentation for yours.
#pragma once

#include "board_dims.h"

#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_RGB _bus;
  lgfx::Panel_RGB _panel;
  lgfx::Light_PWM _light;
  lgfx::Touch_GT911 _touch;

 public:
  LGFX() {
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
      cfg.pin_d0 = GPIO_NUM_8;    // B0
      cfg.pin_d1 = GPIO_NUM_3;    // B1
      cfg.pin_d2 = GPIO_NUM_46;   // B2
      cfg.pin_d3 = GPIO_NUM_9;    // B3
      cfg.pin_d4 = GPIO_NUM_1;    // B4
      cfg.pin_d5 = GPIO_NUM_5;    // G0
      cfg.pin_d6 = GPIO_NUM_6;    // G1
      cfg.pin_d7 = GPIO_NUM_7;    // G2
      cfg.pin_d8 = GPIO_NUM_15;   // G3
      cfg.pin_d9 = GPIO_NUM_16;   // G4
      cfg.pin_d10 = GPIO_NUM_4;   // G5
      cfg.pin_d11 = GPIO_NUM_45;  // R0
      cfg.pin_d12 = GPIO_NUM_48;  // R1
      cfg.pin_d13 = GPIO_NUM_47;  // R2
      cfg.pin_d14 = GPIO_NUM_21;  // R3
      cfg.pin_d15 = GPIO_NUM_14;  // R4
      cfg.pin_henable = GPIO_NUM_40;
      cfg.pin_vsync = GPIO_NUM_41;
      cfg.pin_hsync = GPIO_NUM_39;
      cfg.pin_pclk = GPIO_NUM_42;
      cfg.freq_write = 16000000;
      cfg.hsync_polarity = 0;
      cfg.hsync_front_porch = 8;
      cfg.hsync_pulse_width = 4;
      cfg.hsync_back_porch = 43;
      cfg.vsync_polarity = 0;
      cfg.vsync_front_porch = 8;
      cfg.vsync_pulse_width = 4;
      cfg.vsync_back_porch = 12;
      cfg.pclk_idle_high = 1;
      _bus.config(cfg);
    }
    _panel.setBus(&_bus);
    {
      auto cfg = _light.config();
      cfg.pin_bl = GPIO_NUM_2;
      _light.config(cfg);
    }
    _panel.light(&_light);
    {
      auto cfg = _touch.config();
      cfg.x_min = 0;
      cfg.x_max = board::kWidth - 1;
      cfg.y_min = 0;
      cfg.y_max = board::kHeight - 1;
      cfg.pin_int = -1;
      cfg.pin_rst = 38;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.i2c_port = 1;
      cfg.pin_sda = GPIO_NUM_19;
      cfg.pin_scl = GPIO_NUM_20;
      cfg.freq = 400000;
      cfg.i2c_addr = 0x14;  // GT911 (some boards answer on 0x5D instead)
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};
