#include <stdio.h>

#include "default_theme.h"
#include "gfx/canvas.h"
#include "splash.h"
#include "ui/fade.h"

enum {
    LONG_SIDE = 320,
    SHORT_SIDE = 170,
    PATH_SIZE = 512
};

static Color pixels[LONG_SIDE * SHORT_SIDE];

static int write_ppm(const char *path, const Canvas *canvas)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return 1;
    }
    Rect area = canvas->bounds;
    fprintf(file, "P6\n%d %d\n255\n", area.width, area.height);
    for (int y = 0; y < area.height; y++) {
        for (int x = 0; x < area.width; x++) {
            Color color = canvas_pixel_at(canvas, x, y);
            unsigned char rgb[3] = {(unsigned char)((color >> 11) << 3),
                                    (unsigned char)(((color >> 5) & 0x3F) << 2),
                                    (unsigned char)((color & 0x1F) << 3)};
            fwrite(rgb, 1, sizeof rgb, file);
        }
    }
    return fclose(file) == 0 ? 0 : 1;
}

static Theme light(Theme theme)
{
    theme.colors.background = COLOR_RGB(244, 244, 247);
    theme.colors.text = COLOR_RGB(18, 18, 22);
    return theme;
}

static int render(const char *folder, const char *name, Theme theme)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof path, "%s/%s.ppm", folder, name);
    Rect screen = {0, 0, theme.metrics.screen_width, theme.metrics.screen_height};
    Canvas canvas = canvas_make(pixels, screen);
    canvas_clear(&canvas, theme.colors.background);
    splash_draw(&canvas, screen, &theme, FADE_OPAQUE);
    return write_ppm(path, &canvas);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: splash_preview <folder>\n");
        return 2;
    }
    int failures = 0;
    failures += render(argv[1], "splash_landscape_dark", default_theme_for(false));
    failures += render(argv[1], "splash_landscape_light", light(default_theme_for(false)));
    failures += render(argv[1], "splash_portrait_dark", default_theme_for(true));
    failures += render(argv[1], "splash_portrait_light", light(default_theme_for(true)));
    return failures == 0 ? 0 : 1;
}
