#include "splash.h"

#include "gfx/text.h"
#include "product.h"
#include "splash_logo.h"
#include "ui/label.h"

enum {
    SPLASH_GAP = 12,
    PIXELS_PER_BYTE = 4,
    BITS_PER_PIXEL = 2,
    PIXEL_MASK = 0x03,
    ALPHA_STEP = 5
};

uint8_t splash_logo_alpha(int x, int y)
{
    if (x < 0 || y < 0 || x >= SPLASH_LOGO_WIDTH || y >= SPLASH_LOGO_HEIGHT) {
        return 0;
    }
    uint8_t packed = SPLASH_LOGO_ALPHA[y * SPLASH_LOGO_STRIDE + x / PIXELS_PER_BYTE];
    int shift = (PIXELS_PER_BYTE - 1 - x % PIXELS_PER_BYTE) * BITS_PER_PIXEL;
    return (uint8_t)(((packed >> shift) & PIXEL_MASK) * ALPHA_STEP);
}

SplashLayout splash_layout(Rect screen, const Font *font, const char *word)
{
    int word_width = text_width(font, word);
    if (screen.height > screen.width) {
        int line_height = font_line_height(font);
        int top = screen.y + (screen.height - SPLASH_LOGO_HEIGHT - SPLASH_GAP - line_height) / 2;
        return (SplashLayout){
            .logo = {screen.x + (screen.width - SPLASH_LOGO_WIDTH) / 2, top, SPLASH_LOGO_WIDTH,
                     SPLASH_LOGO_HEIGHT},
            .word = {screen.x + (screen.width - word_width) / 2,
                     top + SPLASH_LOGO_HEIGHT + SPLASH_GAP, word_width, line_height},
        };
    }
    int left = screen.x + (screen.width - SPLASH_LOGO_WIDTH - SPLASH_GAP - word_width) / 2;
    return (SplashLayout){
        .logo = {left, screen.y + (screen.height - SPLASH_LOGO_HEIGHT) / 2, SPLASH_LOGO_WIDTH,
                 SPLASH_LOGO_HEIGHT},
        .word = {left + SPLASH_LOGO_WIDTH + SPLASH_GAP, screen.y, word_width, screen.height},
    };
}

static void draw_logo(Canvas *canvas, Rect logo, Color color, uint8_t alpha)
{
    Rect visible = rect_intersection(canvas->clip, logo);
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        for (int x = visible.x; x < visible.x + visible.width; x++) {
            uint8_t ink = splash_logo_alpha(x - logo.x, y - logo.y);
            if (ink != 0) {
                canvas_blend_pixel(canvas, (Point){x, y}, color,
                                   (uint8_t)(ink * alpha / COLOR_ALPHA_OPAQUE));
            }
        }
    }
}

void splash_draw(Canvas *canvas, Rect screen, const Theme *theme, uint8_t alpha)
{
    if (alpha == 0) {
        return;
    }
    SplashLayout layout = splash_layout(screen, theme->fonts.title, PRODUCT_NAME);
    draw_logo(canvas, layout.logo, theme->colors->text, alpha);
    Color color = color_blend(theme->colors->background, theme->colors->text, alpha);
    TextStyle style = {.font = theme->fonts.title, .color = color};
    label_draw(canvas, style, (Label){.area = layout.word, .text = PRODUCT_NAME});
}
