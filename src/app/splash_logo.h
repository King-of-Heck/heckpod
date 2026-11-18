#pragma once

#include <stdint.h>

enum {
    SPLASH_LOGO_WIDTH = 55,
    SPLASH_LOGO_HEIGHT = 72,
    SPLASH_LOGO_STRIDE = 28
};

extern const uint8_t SPLASH_LOGO_ALPHA[SPLASH_LOGO_STRIDE * SPLASH_LOGO_HEIGHT];
