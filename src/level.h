#ifndef LEVEL_H
#define LEVEL_H

#include "acts.h"
#include "main.h"
#include "tile_config.h"
#include "raylib.h"
#include <stdbool.h>

#define ROOM_VIEW_PADDING_TOP 16.0f

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
    int *surface_y;
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

bool level_load(Level *level, const ActDef *act);
void level_free(Level *level);

bool level_set_active_save(Level *level, int save_index);

TileType level_get_tile(const Level *level, int col, int row);
int level_get_surface_y(const Level *level, int col, int row);

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
