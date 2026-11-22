#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "default_theme.h"
#include "product.h"
#include "splash.h"
#include "splash_logo.h"
#include "ui/fade.h"

enum {
    WIDTH = 320,
    HEIGHT = 170,
    STRIP_ROWS = 16
};

static Color whole[WIDTH * HEIGHT];
static Color strip[WIDTH * STRIP_ROWS];

static const Rect LANDSCAPE = {0, 0, WIDTH, HEIGHT};
static const Rect PORTRAIT = {0, 0, HEIGHT, WIDTH};

static bool inside(Rect inner, Rect outer)
{
    return inner.x >= outer.x && inner.y >= outer.y &&
           inner.x + inner.width <= outer.x + outer.width &&
           inner.y + inner.height <= outer.y + outer.height;
}

static Canvas cleared(Color *pixels, Rect area)
{
    Canvas canvas = canvas_make(pixels, area);
    canvas_clear(&canvas, DEFAULT_THEME.colors->background);
    return canvas;
}

static Point logo_pixel_with(uint8_t alpha)
{
    for (int y = 0; y < SPLASH_LOGO_HEIGHT; y++) {
        for (int x = 0; x < SPLASH_LOGO_WIDTH; x++) {
            if (splash_logo_alpha(x, y) == alpha) {
                return (Point){x, y};
            }
        }
    }
    return (Point){-1, -1};
}

static void the_product_is_called_heckpod(void)
{
    CHECK(strcmp("HeckPod", PRODUCT_NAME) == 0);
}

static void logo_pixels_outside_the_table_are_clear(void)
{
    CHECK_EQUAL(0, splash_logo_alpha(-1, 0));
    CHECK_EQUAL(0, splash_logo_alpha(SPLASH_LOGO_WIDTH, 0));
    CHECK_EQUAL(0, splash_logo_alpha(0, SPLASH_LOGO_HEIGHT));
}

static void the_logo_has_solid_ink_and_clear_paper(void)
{
    CHECK(logo_pixel_with(COLOR_ALPHA_OPAQUE).x >= 0);
    CHECK(logo_pixel_with(0).x >= 0);
}

static void landscape_puts_the_word_right_of_the_logo(void)
{
    SplashLayout layout = splash_layout(LANDSCAPE, DEFAULT_THEME.fonts.title, PRODUCT_NAME);
    CHECK(inside(layout.logo, LANDSCAPE));
    CHECK(inside(layout.word, LANDSCAPE));
    CHECK_EQUAL(SPLASH_LOGO_HEIGHT, layout.logo.height);
    CHECK_EQUAL(layout.logo.x + layout.logo.width + 12, layout.word.x);
}

static void landscape_centres_the_pair(void)
{
    SplashLayout layout = splash_layout(LANDSCAPE, DEFAULT_THEME.fonts.title, PRODUCT_NAME);
    int left = layout.logo.x;
    int right = WIDTH - (layout.word.x + layout.word.width);
    CHECK(abs(left - right) <= 1);
    int top = layout.logo.y;
    int bottom = HEIGHT - (layout.logo.y + layout.logo.height);
    CHECK(abs(top - bottom) <= 1);
}

static void portrait_puts_the_word_under_the_logo(void)
{
    SplashLayout layout = splash_layout(PORTRAIT, DEFAULT_THEME.fonts.title, PRODUCT_NAME);
    CHECK(inside(layout.logo, PORTRAIT));
    CHECK(inside(layout.word, PORTRAIT));
    CHECK_EQUAL(layout.logo.y + layout.logo.height + 12, layout.word.y);
    int top = layout.logo.y;
    int bottom = WIDTH - (layout.word.y + layout.word.height);
    CHECK(abs(top - bottom) <= 1);
}

static void at_full_fade_ink_is_text_and_paper_is_background(void)
{
    Canvas canvas = cleared(whole, LANDSCAPE);
    splash_draw(&canvas, LANDSCAPE, &DEFAULT_THEME, FADE_OPAQUE);
    SplashLayout layout = splash_layout(LANDSCAPE, DEFAULT_THEME.fonts.title, PRODUCT_NAME);
    Point ink = logo_pixel_with(COLOR_ALPHA_OPAQUE);
    Point paper = logo_pixel_with(0);
    CHECK_EQUAL(DEFAULT_THEME.colors->text,
                canvas_pixel_at(&canvas, layout.logo.x + ink.x, layout.logo.y + ink.y));
    CHECK_EQUAL(DEFAULT_THEME.colors->background,
                canvas_pixel_at(&canvas, layout.logo.x + paper.x, layout.logo.y + paper.y));
}

static void at_full_fade_the_word_is_drawn(void)
{
    Canvas canvas = cleared(whole, LANDSCAPE);
    splash_draw(&canvas, LANDSCAPE, &DEFAULT_THEME, FADE_OPAQUE);
    SplashLayout layout = splash_layout(LANDSCAPE, DEFAULT_THEME.fonts.title, PRODUCT_NAME);
    int inked = 0;
    for (int y = layout.word.y; y < layout.word.y + layout.word.height; y++) {
        for (int x = layout.word.x; x < layout.word.x + layout.word.width; x++) {
            inked += canvas_pixel_at(&canvas, x, y) == DEFAULT_THEME.colors->text ? 1 : 0;
        }
    }
    CHECK(inked > 0);
}

static void at_zero_fade_nothing_is_drawn(void)
{
    Canvas canvas = cleared(whole, LANDSCAPE);
    splash_draw(&canvas, LANDSCAPE, &DEFAULT_THEME, 0);
    for (int index = 0; index < WIDTH * HEIGHT; index++) {
        CHECK_EQUAL(DEFAULT_THEME.colors->background, whole[index]);
    }
}

static void drawing_in_strips_matches_drawing_at_once(void)
{
    Canvas canvas = cleared(whole, LANDSCAPE);
    splash_draw(&canvas, LANDSCAPE, &DEFAULT_THEME, FADE_OPAQUE);
    for (int top = 0; top < HEIGHT; top += STRIP_ROWS) {
        int rows = HEIGHT - top < STRIP_ROWS ? HEIGHT - top : STRIP_ROWS;
        Canvas band = cleared(strip, (Rect){0, top, WIDTH, rows});
        splash_draw(&band, LANDSCAPE, &DEFAULT_THEME, FADE_OPAQUE);
        for (int index = 0; index < WIDTH * rows; index++) {
            CHECK_EQUAL(whole[top * WIDTH + index], strip[index]);
        }
    }
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_product_is_called_heckpod),
        TEST_CASE(logo_pixels_outside_the_table_are_clear),
        TEST_CASE(the_logo_has_solid_ink_and_clear_paper),
        TEST_CASE(landscape_puts_the_word_right_of_the_logo),
        TEST_CASE(landscape_centres_the_pair),
        TEST_CASE(portrait_puts_the_word_under_the_logo),
        TEST_CASE(at_full_fade_ink_is_text_and_paper_is_background),
        TEST_CASE(at_full_fade_the_word_is_drawn),
        TEST_CASE(at_zero_fade_nothing_is_drawn),
        TEST_CASE(drawing_in_strips_matches_drawing_at_once),
    };
    splash_logo_copy();
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
