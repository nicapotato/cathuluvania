#ifndef COLLISION_H
#define COLLISION_H

#include "main.h"
#include "raylib.h"
#include <stdbool.h>

struct Level;

typedef enum {
    COLL_EDGE_FLOOR = 0,
    COLL_EDGE_CEILING,
    COLL_EDGE_WALL_LEFT,
    COLL_EDGE_WALL_RIGHT,
} CollisionEdgeKind;

typedef struct CollisionEdge {
    CollisionEdgeKind kind;
    float x0;
    float y0;
    float x1;
    float y1;
} CollisionEdge;

typedef struct CollisionSpatialGrid {
    int cols;
    int rows;
    int *cell_offsets;
    int *cell_indices;
} CollisionSpatialGrid;

typedef struct CollisionWorld {
    CollisionEdge *edges;
    int count;
    int capacity;
    CollisionSpatialGrid grid;
    int *query_stamp;
    int query_generation;
} CollisionWorld;

/* Flat-bottom rounded-top capsule anchored at feet (Hollow Knight style). */
typedef struct PlayerCapsule {
    float feet_x;
    float feet_y;
    float half_w;
    float height;
    float corner_r;
    bool valid;
} PlayerCapsule;

typedef struct PlayerCapsuleParams {
    float half_w;
    float height;
    float corner_r;
    bool valid;
} PlayerCapsuleParams;

typedef struct CollisionMoveResult {
    bool grounded;
    bool wall_left;
    bool wall_right;
} CollisionMoveResult;

void collision_world_init(CollisionWorld *world);
void collision_world_free(CollisionWorld *world);
bool collision_world_build(CollisionWorld *world, const struct Level *level);

void player_capsule_from_params(const PlayerCapsuleParams *params, float feet_x, float feet_y,
                                PlayerCapsule *out);
void player_capsule_from_pos(const PlayerCapsuleParams *params, float px, float py,
                             PlayerCapsule *out);

float player_capsule_pos_y_from_feet(const PlayerCapsuleParams *params, float feet_y);

bool player_capsule_overlaps_aabb(const PlayerCapsule *cap, float l, float t, float r, float b);

CollisionMoveResult collision_move_player(CollisionWorld *world, PlayerCapsule *cap,
                                          float *px, float *py, float *vx, float *vy, float dt);

void collision_resolve_overlap(CollisionWorld *world, PlayerCapsule *cap, float *px,
                               float *py);

bool collision_check_grounded(const CollisionWorld *world, const PlayerCapsule *cap);

void collision_draw_world_debug(const CollisionWorld *world, float cam_x, float cam_y,
                                float view_w, float view_h);
void collision_draw_capsule_debug(const PlayerCapsule *cap);

#endif
