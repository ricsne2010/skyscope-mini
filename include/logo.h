// The Project Moon Pi logo, embedded in the firmware (data/splash.jpg, data/logo.jpg).
#pragma once

#include <cstddef>
#include <cstdint>

namespace logo {
const uint8_t* splashJpg(size_t* len);  // 330 x 330
const uint8_t* smallJpg(size_t* len);   // 96 x 96
}  // namespace logo
