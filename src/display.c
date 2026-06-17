#include "display.h"
#include "main.h"

static int display_min_scale(void) {
#if defined(PLATFORM_WEB)
    return 1;
#else
    return MIN_WINDOW_SCALE;
#endif
}

bool display_init(const char *title) {
    unsigned int flags = FLAG_VSYNC_HINT;
#if !defined(PLATFORM_WEB)
    flags |= FLAG_WINDOW_RESIZABLE;
#endif
#if defined(__APPLE__) && defined(__MACH__)
    flags |= FLAG_WINDOW_HIGHDPI;
#endif
    SetConfigFlags(flags);

    InitWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, title);
    if (!IsWindowReady())
        return false;

#if !defined(PLATFORM_WEB)
    SetWindowMinSize(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
#endif
    return true;
}

DisplayViewport display_viewport(void) {
    DisplayViewport vp = { 0 };
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int min_scale = display_min_scale();

    int scale = sw / VIEW_WIDTH;
    if (sh / VIEW_HEIGHT < scale)
        scale = sh / VIEW_HEIGHT;
    if (scale < min_scale)
        scale = min_scale;

    vp.scale = scale;
    vp.draw_w = VIEW_WIDTH * scale;
    vp.draw_h = VIEW_HEIGHT * scale;
    vp.draw_x = (sw - vp.draw_w) / 2;
    vp.draw_y = (sh - vp.draw_h) / 2;
    return vp;
}

void display_handle_input(void) {
#if !defined(PLATFORM_WEB)
    if (IsKeyPressed(KEY_F11))
        ToggleFullscreen();

    if (IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))
        ToggleBorderlessWindowed();
#endif
}
