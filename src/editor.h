#ifndef EDITOR_H
#define EDITOR_H

#include "main.h"
#include <stdbool.h>

struct Game;

typedef enum {
    EDITOR_TOOL_BRUSH = 0,
    EDITOR_TOOL_ERASE,
    EDITOR_TOOL_SELECT,
} EditorTool;

typedef struct EditorCellKey {
    int col;
    int row;
} EditorCellKey;

typedef struct EditorState {
    bool active;
    EditorTool tool;
    int brush_type_index;
    bool dirty;
    bool mouse_down;
    int last_paint_col;
    int last_paint_row;
    EditorCellKey *selection;
    int selection_count;
    int selection_cap;
    float saved_camera_x;
    float saved_camera_y;
} EditorState;

void editor_init(EditorState *editor);
void editor_free(EditorState *editor);

void editor_enter(struct Game *g);
void editor_exit(struct Game *g);

void editor_handle_input(struct Game *g, float dt);
void editor_draw_world(const struct Game *g);
void editor_draw_ui(const struct Game *g);

bool editor_try_exit(struct Game *g);

bool editor_create_new_act(struct Game *g);

#endif
