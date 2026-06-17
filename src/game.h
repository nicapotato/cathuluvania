#ifndef GAME_H
#define GAME_H

#include "acts.h"
#include "level.h"
#include "player_sprite.h"
#include "raylib-aseprite.h"
#include <stdbool.h>

typedef enum {
    PLAYER_ANIM_IDLE = 0,
    PLAYER_ANIM_RUN,
    PLAYER_ANIM_JUMP,
    PLAYER_ANIM_JUMP_IN,
    PLAYER_ANIM_FALL,
    PLAYER_ANIM_ATTACK,
    PLAYER_ANIM_GLIDE,
    PLAYER_ANIM_DASH,
} PlayerAnimKind;

typedef struct Player {
    Vector2 pos;
    Vector2 vel;
    Vector2 spawn;
    bool grounded;
    bool on_wall_left;
    bool on_wall_right;
    float coyote_timer;
    float jump_buffer_timer;
    int air_jumps_left;
    int facing;
    PlayerAnimKind anim_kind;
    AsepriteTag tag;
    bool attack_active;
    bool dash_active;
    bool gliding;
    float jump_in_timer;
} Player;

typedef struct CameraState {
    float x;
    float y;
} CameraState;

typedef enum {
    TRANS_NONE = 0,
    TRANS_CAMERA_PAN,
    TRANS_FADE_OUT,
    TRANS_FADE_IN,
} TransitionPhase;

typedef enum {
    PENDING_NONE = 0,
    PENDING_TUNNEL,
    PENDING_TELEPORT,
} PendingTransitionKind;

typedef struct Game {
    Level level;
    Player player;
    PlayerSprite player_sprite;
    CameraState camera;
    RenderTexture2D target;
    bool target_loaded;
    bool running;
    int active_act_index;
    bool act_menu_open;
    bool save_menu_open;
    bool map_open;
    bool debug_mode;
    int debug_save_index;
    TransitionPhase transition;
    float transition_alpha;
    float player_pan_from_x;
    float player_pan_from_y;
    float player_pan_to_x;
    float player_pan_to_y;
    float camera_pan_t;
    PendingTransitionKind pending_kind;
    const TunnelDef *pending_tunnel;
    const char *pending_target_room_id;
    const TeleportDef *pending_from_teleport;
    const TeleportDef *pending_to_teleport;
    const TunnelDef *trigger_overlap_tunnel;
    const TeleportDef *trigger_overlap_teleport;
} Game;

bool game_new(Game **out);
void game_free(Game **game);
bool game_run(Game *game);

#endif
