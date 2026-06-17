#ifndef DISPLAY_H
#define DISPLAY_H

#include "raylib.h"

typedef struct {
    int draw_x;
    int draw_y;
    int draw_w;
    int draw_h;
    int scale;
} DisplayViewport;

bool display_init(const char *title);
DisplayViewport display_viewport(void);
void display_handle_input(void);

#endif
