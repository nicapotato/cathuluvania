#ifndef LEVEL_H
#define LEVEL_H

#include "acts.h"
#include "act_registry.h"
#include "gameplay_grid.h"
#include "main.h"
#include "tile_catalog.h"
#include "tile_config.h"
#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>

#define ROOM_VIEW_PADDING_TOP 16.0f
#define LEVEL_GAMEPLAY_PATH_LEN 256
#define LEVEL_ASEPRITE_PATH_LEN 256

typedef enum {
    ZONE_NONE = 0,
    ZONE_ROOM,
    ZONE_TUNNEL,
} LevelZoneKind;

typedef struct LevelZone {
    LevelZoneKind kind;
    int index;
    const char *id;
} LevelZone;

typedef struct Level {
    int width;
    int height;
    int cols;
    int rows;
    TileType *tiles;
    uint32_t *tile_flags;
    int *surface_y;
    GameplayGrid gameplay;
    char gameplay_path[LEVEL_GAMEPLAY_PATH_LEN];
    char aseprite_path[LEVEL_ASEPRITE_PATH_LEN];
    char collision_png_path[LEVEL_GAMEPLAY_PATH_LEN];
    char primitives_png_path[LEVEL_GAMEPLAY_PATH_LEN];
    Image collision_baked;
    Image collision_edit;
    bool collision_dirty;
    char act_id[ACT_DESC_ID_LEN];
    Texture2D tex_background;
    Texture2D tex_base;
    Vector2 spawn;
    Vector2 spawn_slice;
    const SavePointDef *saves;
    int save_count;
    int active_save_index;
    const RoomDef *rooms;
    float *room_view_y;
    float *room_view_h;
    int room_count;
    const TunnelDef *tunnels;
    int tunnel_count;
    const TeleportDef *teleports;
    int teleport_count;
    int active_room_index;
    bool loaded;
} Level;

bool level_load(Level *level, const ActDesc *act);
void level_free(Level *level);

bool level_set_active_save(Level *level, int save_index);

TileType level_get_tile(const Level *level, int col, int row);
uint32_t level_get_cell_flags(const Level *level, int col, int row);
bool level_cell_has_flag(const Level *level, int col, int row, uint32_t flag);
int level_get_surface_y(const Level *level, int col, int row);

void level_sync_collision_from_gameplay(Level *level, const TileCatalog *catalog);
bool level_save_gameplay(const Level *level, const TileCatalog *catalog);

bool level_cell_is_solid(const Level *level, int col, int row);
bool level_cell_is_primitive_only(const Level *level, int col, int row);
void level_draw_primitive_overlay(const Level *level, float cam_x, float cam_y, float view_w,
                                  float view_h, Color fill, Color outline);
uint32_t level_cell_effective_flags(const Level *level, const TileCatalog *catalog, int col, int row);
bool level_has_tag_override(const Level *level, int col, int row);

void level_collision_paint_cell(Level *level, int col, int row);
void level_collision_erase_cell(Level *level, int col, int row);
void level_sync_from_collision_image(Level *level, const TileCatalog *catalog);
void level_reload_collision_edit(Level *level, const TileCatalog *catalog);
void level_refresh_collision_texture(Level *level, const TileCatalog *catalog);
void level_rebind_act(Level *level, const ActDesc *act);
bool level_reload_visuals(Level *level, const ActDesc *act);
bool level_save_collision_to_aseprite(Level *level);

const RoomDef *level_get_active_room(const Level *level);
const TunnelDef *level_find_tunnel_by_id(const Level *level, const char *tunnel_id);
const TeleportDef *level_find_teleport_by_id(const Level *level, const char *teleport_id);
const char *level_tunnel_other_room(const TunnelDef *tunnel, const char *current_room_id);
int level_find_room_index_by_id(const Level *level, const char *room_id);
int level_find_room_at(const Level *level, float x, float y);
bool level_place_at_tunnel(const Level *level, const TunnelDef *tunnel, const char *from_room_id,
                           const char *to_room_id, Vector2 *out_pos);
bool level_place_at_teleport(const Level *level, const TeleportDef *to, Vector2 *out_pos);

float level_room_view_y(const Level *level, int room_index);
float level_room_view_h(const Level *level, int room_index);
float level_active_view_y(const Level *level);
float level_active_view_h(const Level *level);

LevelZone level_zone_at(const Level *level, float x, float y);
LevelZone level_get_active_zone(const Level *level);

#endif
