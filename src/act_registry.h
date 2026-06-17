#ifndef ACT_REGISTRY_H
#define ACT_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

#define ACT_DESC_PATH_LEN 256
#define ACT_DESC_ID_LEN 64
#define ACT_DESC_LABEL_LEN 128

typedef struct SavePointDef {
    int index;
    float x, y;
    const char *room_id;
} SavePointDef;

typedef struct RoomDef {
    const char *id;
    const char *name;
    bool isolated;
    float x, y, w, h;
    float view_y, view_h;
} RoomDef;

typedef struct TunnelDef {
    const char *id;
    const char *room_a_id;
    const char *room_b_id;
    float x, y, w, h;
    float spawn_ax, spawn_ay;
    float spawn_bx, spawn_by;
} TunnelDef;

typedef struct TeleportDef {
    const char *id;
    const char *room_id;
    const char *link_id;
    const char *name;
    float x, y, w, h;
    float spawn_x, spawn_y;
} TeleportDef;

typedef struct ActDesc {
    char id[ACT_DESC_ID_LEN];
    char label[ACT_DESC_LABEL_LEN];
    int width;
    int height;
    int cols;
    int rows;
    char background_png[ACT_DESC_PATH_LEN];
    char collision_png[ACT_DESC_PATH_LEN];
    char gameplay_json[ACT_DESC_PATH_LEN];
    char aseprite_path[ACT_DESC_PATH_LEN];
    SavePointDef *saves;
    int save_count;
    RoomDef *rooms;
    int room_count;
    TunnelDef *tunnels;
    int tunnel_count;
    TeleportDef *teleports;
    int teleport_count;
    char **owned_strings;
    int owned_string_count;
} ActDesc;

typedef struct ActRegistry {
    ActDesc *acts;
    int count;
} ActRegistry;

bool act_registry_load(ActRegistry *reg, const char *manifest_path);
void act_registry_free(ActRegistry *reg);
bool act_registry_reload(ActRegistry *reg);

int act_registry_count(const ActRegistry *reg);
const ActDesc *act_registry_get(const ActRegistry *reg, int index);
const ActDesc *act_registry_find(const ActRegistry *reg, const char *id);
int act_registry_index_of(const ActRegistry *reg, const char *id);

bool act_registry_append_manifest_entry(const char *manifest_path, const char *id, const char *label);

#endif
