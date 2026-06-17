#include "editor.h"

#include "act_registry.h"
#include "game.h"
#include "act_create.h"
#include "gameplay_grid.h"
#include "level.h"
#include "act_registry.h"
#include "tile_catalog.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EDITOR_PAN_SPEED 420.0f
#define EDITOR_UI_FONT 12
#define EDITOR_TAG_PANEL_W 160
#define EDITOR_TAG_PANEL_PAD 8
#define EDITOR_TAG_ROW_H 20
#define EDITOR_MAX_SELECTION 4096

static bool ctrl_down(void) {
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)
        || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
}

static bool shift_down(void) {
    return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
}

static Vector2 editor_screen_to_world(const Game *g, Vector2 screen) {
    float scale = (float)WINDOW_WIDTH / (float)VIEW_WIDTH;
    float tx = screen.x / scale;
    float ty = screen.y / scale;
    return (Vector2){ g->camera.x + tx, g->camera.y + ty };
}

static bool editor_world_to_cell(const Level *level, Vector2 world, int *out_col, int *out_row) {
    if (!level)
        return false;

    int col = (int)floorf(world.x / (float)TILE_SIZE);
    int row = (int)floorf(world.y / (float)TILE_SIZE);
    if (col < 0 || col >= level->cols || row < 0 || row >= level->rows)
        return false;

    if (out_col)
        *out_col = col;
    if (out_row)
        *out_row = row;
    return true;
}

static void editor_clamp_camera(Game *g) {
    float view_w = (float)VIEW_WIDTH;
    float view_h = (float)VIEW_HEIGHT;
    float max_x = (float)g->level.width - view_w;
    float max_y = (float)g->level.height - view_h;
    if (max_x < 0.0f)
        max_x = 0.0f;
    if (max_y < 0.0f)
        max_y = 0.0f;
    if (g->camera.x < 0.0f)
        g->camera.x = 0.0f;
    if (g->camera.y < 0.0f)
        g->camera.y = 0.0f;
    if (g->camera.x > max_x)
        g->camera.x = max_x;
    if (g->camera.y > max_y)
        g->camera.y = max_y;
}

static void editor_clear_selection(EditorState *editor) {
    editor->selection_count = 0;
}

static void editor_toggle_selection(EditorState *editor, int col, int row) {
    for (int i = 0; i < editor->selection_count; i++) {
        if (editor->selection[i].col == col && editor->selection[i].row == row) {
            editor->selection[i] = editor->selection[editor->selection_count - 1];
            editor->selection_count--;
            return;
        }
    }

    if (editor->selection_count >= editor->selection_cap)
        return;

    editor->selection[editor->selection_count].col = col;
    editor->selection[editor->selection_count].row = row;
    editor->selection_count++;
}

static void editor_select_one(EditorState *editor, int col, int row) {
    editor_clear_selection(editor);
    editor_toggle_selection(editor, col, row);
}

static int editor_solid_type_index(const TileCatalog *catalog) {
    int idx = tile_catalog_find_type_index(catalog, "solid");
    return idx >= 0 ? idx : 0;
}

static void editor_apply_brush(Game *g, int col, int row) {
    EditorState *editor = &g->editor;

    if (editor->tool == EDITOR_TOOL_ERASE) {
        if (level_cell_is_solid(&g->level, col, row) || g->level.collision_edit.data) {
            level_collision_erase_cell(&g->level, col, row);
            editor->dirty = true;
        }
        return;
    }

    if (editor->tool == EDITOR_TOOL_BRUSH) {
        level_collision_paint_cell(&g->level, col, row);
        editor->dirty = true;
    }
}

static void editor_sync_level(Game *g) {
    const TileCatalog *catalog = tile_catalog_global();
    if (!catalog)
        return;
    level_refresh_collision_texture(&g->level, catalog);
}

static bool editor_point_in_tag_panel(const Game *g, Vector2 mouse, Rectangle *out_panel) {
    const TileCatalog *catalog = tile_catalog_global();
    if (!catalog || g->editor.selection_count <= 0)
        return false;

    int rows = catalog->flag_count;
    float h = (float)EDITOR_TAG_PANEL_PAD * 2.0f + 18.0f + (float)rows * (float)EDITOR_TAG_ROW_H;
    float w = (float)EDITOR_TAG_PANEL_W;
    float x = (float)GetScreenWidth() - w - 8.0f;
    float y = 48.0f;

    Rectangle panel = { x, y, w, h };
    if (out_panel)
        *out_panel = panel;
    return CheckCollisionPointRec(mouse, panel);
}

static void editor_ensure_tag_cell(GameplayGrid *grid, int col, int row, int solid_index) {
    GameplayCell *cell = gameplay_grid_get_mut(grid, col, row);
    if (!cell)
        return;
    if (cell->type_index < 0)
        cell->type_index = (int16_t)solid_index;
}

static void editor_handle_tag_panel_click(Game *g, Vector2 mouse) {
    const TileCatalog *catalog = tile_catalog_global();
    EditorState *editor = &g->editor;
    Rectangle panel;

    if (!catalog || editor->selection_count <= 0 || !editor_point_in_tag_panel(g, mouse, &panel))
        return;

    float rel_y = mouse.y - panel.y - (float)EDITOR_TAG_PANEL_PAD - 18.0f;
    int row = (int)(rel_y / (float)EDITOR_TAG_ROW_H);
    if (row < 0 || row >= catalog->flag_count)
        return;

    uint32_t bit = catalog->flags[row].bit;
    int solid_index = editor_solid_type_index(catalog);
    bool all_have = true;

    for (int i = 0; i < editor->selection_count; i++) {
        const EditorCellKey *key = &editor->selection[i];
        if (!level_cell_is_solid(&g->level, key->col, key->row)) {
            all_have = false;
            break;
        }
        if ((level_cell_effective_flags(&g->level, catalog, key->col, key->row) & bit) == 0) {
            all_have = false;
            break;
        }
    }

    for (int i = 0; i < editor->selection_count; i++) {
        const EditorCellKey *key = &editor->selection[i];
        if (!level_cell_is_solid(&g->level, key->col, key->row))
            continue;

        editor_ensure_tag_cell(&g->level.gameplay, key->col, key->row, solid_index);

        uint32_t flags = level_cell_effective_flags(&g->level, catalog, key->col, key->row);
        if (all_have)
            flags &= ~bit;
        else
            flags |= bit;

        gameplay_grid_set_flags_override(&g->level.gameplay, catalog, key->col, key->row, flags);
    }

    editor->dirty = true;
    editor_sync_level(g);
}

void editor_init(EditorState *editor) {
    if (!editor)
        return;
    memset(editor, 0, sizeof(*editor));
    editor->tool = EDITOR_TOOL_SELECT;
    editor->selection_cap = EDITOR_MAX_SELECTION;
    editor->selection = (EditorCellKey *)calloc((size_t)editor->selection_cap, sizeof(EditorCellKey));
}

void editor_free(EditorState *editor) {
    if (!editor)
        return;
    free(editor->selection);
    editor->selection = NULL;
    editor->selection_count = 0;
    editor->selection_cap = 0;
}

void editor_enter(Game *g) {
    if (!g)
        return;

    EditorState *editor = &g->editor;
    editor->active = true;
    editor->tool = EDITOR_TOOL_BRUSH;
    editor->saved_camera_x = g->camera.x;
    editor->saved_camera_y = g->camera.y;
    editor->mouse_down = false;
    editor->last_paint_col = -1;
    editor->last_paint_row = -1;
    editor_clear_selection(editor);
    g->map_open = false;
    g->act_menu_open = false;
    g->save_menu_open = false;
}

void editor_exit(Game *g) {
    if (!g)
        return;

    g->editor.active = false;
    g->editor.mouse_down = false;
    editor_clear_selection(&g->editor);
}

bool editor_try_exit(Game *g) {
    if (!g || !g->editor.active)
        return true;

    if (g->editor.dirty) {
        TraceLog(LOG_WARNING, "Editor: unsaved changes (Ctrl+S to save)");
    }

    editor_exit(g);
    return true;
}

static void editor_save(Game *g) {
    const TileCatalog *catalog = tile_catalog_global();
    if (!catalog)
        return;

    level_sync_from_collision_image(&g->level, catalog);

    bool ok_tags = level_save_gameplay(&g->level, catalog);
    bool ok_collision = true;

    if (g->level.collision_dirty) {
        if (g->level.act_id[0] == '\0') {
            TraceLog(LOG_WARNING, "Editor: no act id — collision not saved");
            ok_collision = false;
        } else {
            ok_collision = level_save_collision_to_aseprite(&g->level);
            if (ok_collision)
                level_sync_from_collision_image(&g->level, catalog);
        }
    }

    if (ok_tags && ok_collision) {
        g->editor.dirty = false;
        TraceLog(LOG_INFO, "Editor: saved tags + collision for %s", g->level.act_id);
    } else {
        TraceLog(LOG_WARNING, "Editor: save incomplete (tags=%d collision=%d)", ok_tags, ok_collision);
    }
}

bool editor_create_new_act(Game *g) {
    if (!g)
        return false;
    return game_create_new_act(g);
}

void editor_handle_input(Game *g, float dt) {
    if (!g || !g->editor.active)
        return;

    EditorState *editor = &g->editor;

    if (IsKeyPressed(KEY_N) && ctrl_down()) {
        if (editor_create_new_act(g)) {
            editor_enter(g);
            g->editor.dirty = false;
            TraceLog(LOG_INFO, "Editor: created and loaded new act");
        }
    }

    if (IsKeyPressed(KEY_B))
        editor->tool = EDITOR_TOOL_BRUSH;
    if (IsKeyPressed(KEY_E))
        editor->tool = EDITOR_TOOL_ERASE;
    if (IsKeyPressed(KEY_S) && !ctrl_down())
        editor->tool = EDITOR_TOOL_SELECT;

    if (ctrl_down() && IsKeyPressed(KEY_S))
        editor_save(g);

    if (IsKeyPressed(KEY_ESCAPE)) {
        editor_try_exit(g);
        return;
    }

    float pan_x = 0.0f;
    float pan_y = 0.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
        pan_x -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
        pan_x += 1.0f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
        pan_y -= 1.0f;
    if (IsKeyDown(KEY_S) && !ctrl_down())
        pan_y += 1.0f;

    g->camera.x += pan_x * EDITOR_PAN_SPEED * dt;
    g->camera.y += pan_y * EDITOR_PAN_SPEED * dt;
    editor_clamp_camera(g);

    Vector2 mouse = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (editor_point_in_tag_panel(g, mouse, NULL)) {
            editor_handle_tag_panel_click(g, mouse);
            return;
        }

        Vector2 world = editor_screen_to_world(g, mouse);
        int col, row;
        if (!editor_world_to_cell(&g->level, world, &col, &row))
            return;

        editor->mouse_down = true;
        editor->last_paint_col = -1;
        editor->last_paint_row = -1;

        if (editor->tool == EDITOR_TOOL_SELECT) {
            if (shift_down()) {
                if (level_cell_is_solid(&g->level, col, row))
                    editor_toggle_selection(editor, col, row);
            } else {
                if (level_cell_is_solid(&g->level, col, row))
                    editor_select_one(editor, col, row);
                else
                    editor_clear_selection(editor);
            }
        } else {
            editor_apply_brush(g, col, row);
            editor_sync_level(g);
            editor->last_paint_col = col;
            editor->last_paint_row = row;
        }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && editor->mouse_down
        && editor->tool != EDITOR_TOOL_SELECT) {
        Vector2 world = editor_screen_to_world(g, mouse);
        int col, row;
        if (editor_world_to_cell(&g->level, world, &col, &row)) {
            if (col != editor->last_paint_col || row != editor->last_paint_row) {
                editor_apply_brush(g, col, row);
                editor_sync_level(g);
                editor->last_paint_col = col;
                editor->last_paint_row = row;
            }
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        editor->mouse_down = false;
}

void editor_draw_world(const Game *g) {
    if (!g || !g->editor.active)
        return;

    const Level *level = &g->level;
    const TileCatalog *catalog = tile_catalog_global();
    if (!catalog)
        return;

    int c0 = (int)floorf(g->camera.x / (float)TILE_SIZE) - 1;
    int c1 = (int)floorf((g->camera.x + (float)VIEW_WIDTH) / (float)TILE_SIZE) + 1;
    int r0 = (int)floorf(g->camera.y / (float)TILE_SIZE) - 1;
    int r1 = (int)floorf((g->camera.y + (float)VIEW_HEIGHT) / (float)TILE_SIZE) + 1;

    if (c0 < 0)
        c0 = 0;
    if (c1 >= level->cols)
        c1 = level->cols - 1;
    if (r0 < 0)
        r0 = 0;
    if (r1 >= level->rows)
        r1 = level->rows - 1;

    level_draw_primitive_overlay(level, g->camera.x, g->camera.y, (float)VIEW_WIDTH,
                                 (float)VIEW_HEIGHT, (Color){ 255, 255, 255, 200 },
                                 (Color){ 255, 255, 255, 255 });

    for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
            if (!level_cell_is_solid(level, c, r) || level_cell_is_primitive_only(level, c, r))
                continue;

            float x = (float)(c * TILE_SIZE);
            float y = (float)(r * TILE_SIZE);
            Color fill = (Color){ 0, 0, 0, 60 };
            DrawRectangle((int)x, (int)y, TILE_SIZE, TILE_SIZE, fill);
            DrawRectangleLines((int)x, (int)y, TILE_SIZE, TILE_SIZE, (Color){ 255, 255, 255, 60 });

            uint32_t flags = level_cell_effective_flags(level, catalog, c, r);
            if (flags & TILE_FLAG_SLIPPERY)
                DrawRectangle((int)x + 2, (int)y + 2, 4, 4, (Color){ 120, 200, 255, 220 });
            if (flags & TILE_FLAG_CLIMB)
                DrawRectangle((int)x + TILE_SIZE - 6, (int)y + 2, 4, 4, (Color){ 255, 180, 60, 220 });
        }
    }

    for (int i = 0; i < g->editor.selection_count; i++) {
        int c = g->editor.selection[i].col;
        int r = g->editor.selection[i].row;
        float x = (float)(c * TILE_SIZE);
        float y = (float)(r * TILE_SIZE);
        DrawRectangleLinesEx((Rectangle){ x, y, (float)TILE_SIZE, (float)TILE_SIZE }, 2.0f,
                             (Color){ 255, 255, 0, 255 });
    }

    int hover_col, hover_row;
    Vector2 world = editor_screen_to_world(g, GetMousePosition());
    if (editor_world_to_cell(level, world, &hover_col, &hover_row)) {
        float x = (float)(hover_col * TILE_SIZE);
        float y = (float)(hover_row * TILE_SIZE);
        if (g->editor.tool == EDITOR_TOOL_BRUSH && !level_cell_is_solid(level, hover_col, hover_row)) {
            DrawRectangle((int)x, (int)y, TILE_SIZE, TILE_SIZE, (Color){ 255, 255, 255, 80 });
        }
        DrawRectangleLinesEx((Rectangle){ x, y, (float)TILE_SIZE, (float)TILE_SIZE }, 1.0f,
                             (Color){ 255, 255, 255, 200 });
    }
}

static const char *editor_tool_name(EditorTool tool) {
    switch (tool) {
    case EDITOR_TOOL_BRUSH:
        return "Paint";
    case EDITOR_TOOL_ERASE:
        return "Erase";
    case EDITOR_TOOL_SELECT:
        return "Select/Tag";
    default:
        return "?";
    }
}

void editor_draw_ui(const Game *g) {
    if (!g || !g->editor.active)
        return;

    const TileCatalog *catalog = tile_catalog_global();
    const EditorState *editor = &g->editor;
    const Level *level = &g->level;

    DrawRectangle(0, 0, GetScreenWidth(), 52, (Color){ 20, 20, 40, 230 });
    const char *dirty = editor->dirty ? " *" : "";
    const char *col_dirty = level->collision_dirty ? " [collision]" : "";
    DrawText(TextFormat("EDITOR | %s%s%s | Tab/Esc exit | Ctrl+S save",
                        editor_tool_name(editor->tool), dirty, col_dirty),
             8, 8, EDITOR_UI_FONT, RAYWHITE);
    DrawText("B paint | E erase | S tag | Ctrl+S -> layers/<act>-primitives.png | WASD pan",
             8, 24, 10, LIGHTGRAY);
    DrawText("White blocks = editor collision (not tile art). Player can stand on them after Tab exit.",
             8, 38, 10, (Color){ 200, 200, 200, 255 });

    if (catalog && editor->selection_count > 0) {
        Rectangle panel;
        editor_point_in_tag_panel(g, (Vector2){ 0.0f, 0.0f }, &panel);
        DrawRectangleRec(panel, (Color){ 20, 20, 40, 230 });
        DrawRectangleLinesEx(panel, 1.0f, RAYWHITE);
        DrawText(TextFormat("Tags (%d cells)", editor->selection_count),
                 (int)panel.x + EDITOR_TAG_PANEL_PAD, (int)panel.y + EDITOR_TAG_PANEL_PAD,
                 EDITOR_UI_FONT, RAYWHITE);

        for (int i = 0; i < catalog->flag_count; i++) {
            uint32_t bit = catalog->flags[i].bit;
            bool all_on = true;
            for (int s = 0; s < editor->selection_count; s++) {
                const EditorCellKey *key = &editor->selection[s];
                if (!level_cell_is_solid(level, key->col, key->row)) {
                    all_on = false;
                    break;
                }
                if ((level_cell_effective_flags(level, catalog, key->col, key->row) & bit) == 0) {
                    all_on = false;
                    break;
                }
            }

            float row_y = panel.y + (float)EDITOR_TAG_PANEL_PAD + 18.0f + (float)i * (float)EDITOR_TAG_ROW_H;
            DrawText(all_on ? "[x]" : "[ ]", (int)panel.x + EDITOR_TAG_PANEL_PAD, (int)row_y, 11,
                     all_on ? LIME : LIGHTGRAY);
            DrawText(catalog->flags[i].label,
                     (int)panel.x + EDITOR_TAG_PANEL_PAD + 28, (int)row_y, 11, RAYWHITE);
        }
    }
}
