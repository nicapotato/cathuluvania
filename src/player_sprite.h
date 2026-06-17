#ifndef PLAYER_SPRITE_H
#define PLAYER_SPRITE_H

#include "collision.h"
#include "raylib-aseprite.h"
#include "main.h"
#include <stdbool.h>

#define PLAYER_SPRITE_COLLISION_LAYER "collision"
#define PLAYER_SPRITE_BACKGROUND_LAYER "background"
#define PLAYER_SPRITE_ASSET_PATH "resources/visual/character-player.aseprite"

typedef struct PlayerFrameAnchor {
    float center_x;
    float bottom_y;
    float bbox_l;
    float bbox_t;
    float bbox_r;
    float bbox_b;
    bool valid;
} PlayerFrameAnchor;

typedef struct PlayerCollisionOval {
    float cx;
    float cy;
    float rx;
    float ry;
    bool valid;
} PlayerCollisionOval;

typedef struct PlayerSprite {
    Aseprite aseprite;
    PlayerFrameAnchor *frame_anchors;
    int frame_count;
    PlayerFrameAnchor fallback_anchor;
    PlayerCapsuleParams capsule_params;
    bool loaded;
} PlayerSprite;

bool player_sprite_load(PlayerSprite *sprite, const char *path);
void player_sprite_unload(PlayerSprite *sprite);
const PlayerFrameAnchor *player_sprite_frame_anchor(const PlayerSprite *sprite, int frame);
bool player_sprite_world_collision_aabb(const PlayerSprite *sprite, int frame, bool flip_h,
                                        float world_feet_x, float world_feet_y,
                                        float *out_l, float *out_t, float *out_r, float *out_b);
bool player_sprite_world_collision_oval(const PlayerSprite *sprite, int frame, bool flip_h,
                                        float world_feet_x, float world_feet_y,
                                        PlayerCollisionOval *out);
bool player_frame_anchor_from_image(const Image *image, PlayerFrameAnchor *out);
void player_sprite_draw_tag(const PlayerSprite *sprite, AsepriteTag tag, Vector2 world_feet,
                            bool flip_h, Color tint);

#endif
