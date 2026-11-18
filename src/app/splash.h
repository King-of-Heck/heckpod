#pragma once

#include <stdint.h>

#include "gfx/canvas.h"
#include "gfx/font.h"
#include "ui/theme.h"

typedef struct SplashLayout {
    Rect logo;
    Rect word;
} SplashLayout;

SplashLayout splash_layout(Rect screen, const Font *font, const char *word);
uint8_t splash_logo_alpha(int x, int y);
void splash_draw(Canvas *canvas, Rect screen, const Theme *theme, uint8_t alpha);
