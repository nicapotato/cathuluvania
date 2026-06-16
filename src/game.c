#include "game.h"
#include "acts_metadata.h"
#include "tile_config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Screen-space UI (fixed pixel size, not scaled with game viewport). */
#define UI_FONT_SIZE 14
#define UI_ACT_MENU_X 8
#define UI_ACT_MENU_Y 8
#define UI_ACT_MENU_W 118
#define UI_ACT_MENU_ROW_H 20
#define UI_ACT_MENU_PAD 4
#define UI_DEBUG_BTN_W 72
#define UI_DEBUG_BTN_H 22
#define UI_DEBUG_BTN_PAD 8
#define UI_MAP_BTN_W 56
#define UI_MAP_BTN_H 22
#define UI_MAP_BTN_GAP 4
#define UI_MAP_SCREEN_FRACTION 0.8f
#define UI_MAP_PANEL_PAD 12
#define UI_MAP_TITLE_H 22
#define UI_MAP_DOOR_MIN_PX 3.0f
#define UI_SAVE_MENU_GAP 4

#define TRANS_FADE_SPEED 480.0f
#define TUNNEL_PAN_DURATION 0.5f

static float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static bool camera_axis_narrow(float content_origin, float content_size, float view_size,
                               float *out_camera) {
    if (content_size <= view_size) {
        *out_camera = content_origin + (content_size - view_size) * 0.5f;
        return true;
    }
    return false;
}

static void game_clamp_camera_axis(float player_center, float view_size, float content_origin,
                                   float content_size, float *camera) {
    if (camera_axis_narrow(content_origin, content_size, view_size, camera))
        return;

    float min = content_origin;
    float max = content_origin + content_size - view_size;
    *camera = player_center - view_size * 0.5f;
    if (*camera < min)
        *camera = min;
    if (*camera > max)
        *camera = max;
}

static void game_clamp_camera_to_player(Game *g) {
    float view_w = (float)VIEW_WIDTH;
    float view_h = (float)VIEW_HEIGHT;
    const RoomDef *room = level_get_active_room(&g->level);
    if (!room)
        return;

    float content_y = level_active_view_y(&g->level);
    float content_h = level_active_view_h(&g->level);
    game_clamp_camera_axis(g->player.pos.x, view_w, room->x, room->w, &g->camera.x);
    game_clamp_camera_axis(g->player.pos.y, view_h, content_y, content_h, &g->camera.y);
}

static void game_clamp_camera(Game *g) {
    if (g->transition == TRANS_CAMERA_PAN)
        return;

    const RoomDef *room = level_get_active_room(&g->level);
    if (room) {
        game_clamp_camera_to_player(g);
        return;
    }

    float view_w = (float)VIEW_WIDTH;
    float view_h = (float)VIEW_HEIGHT;
    float min_x = 0.0f;
    float max_x = (float)g->level.width - view_w;
    float min_y = 0.0f;
    float max_y = (float)g->level.height - view_h;

    g->camera.x = g->player.pos.x - view_w * 0.5f;
    if (g->camera.x < min_x)
        g->camera.x = min_x;
    if (g->camera.x > max_x)
        g->camera.x = max_x;

    g->camera.y = g->player.pos.y - view_h * 0.5f;
    if (g->camera.y < min_y)
        g->camera.y = min_y;
    if (g->camera.y > max_y)
        g->camera.y = max_y;
}

static bool aabb_overlap(float a_l, float a_t, float a_r, float a_b,
                         float b_l, float b_t, float b_r, float b_b) {
    return a_l < b_r && a_r > b_l && a_t < b_b && a_b > b_t;
}

static void get_player_aabb(float px, float py, float *l, float *t, float *r, float *b) {
    *l = px - PLAYER_HALF;
    *r = px + PLAYER_HALF;
    *t = py - PLAYER_HALF;
    *b = py + PLAYER_HALF;
}

static void get_player_aabb_horizontal(float px, float py, float *l, float *t, float *r, float *b) {
    float cy = py + PLAYER_COLLIDE_Y_BIAS;
    *l = px - PLAYER_HALF_X;
    *r = px + PLAYER_HALF_X;
    *t = cy - PLAYER_HALF_Y;
    *b = cy + PLAYER_HALF_Y;
}

static void get_tile_bounds_px(int col, int row, float *l, float *t, float *r, float *b) {
    *l = (float)(col * TILE_SIZE);
    *t = (float)(row * TILE_SIZE);
    *r = (float)((col + 1) * TILE_SIZE);
    *b = (float)((row + 1) * TILE_SIZE);
}

static float get_tile_feet_y(const Game *g, int col, int row) {
    int surf = level_get_surface_y(&g->level, col, row);
    if (surf >= 0)
        return (float)surf;
    return (float)(row * TILE_SIZE);
}

static TileType get_tile_at(const Game *g, int col, int row) {
    return level_get_tile(&g->level, col, row);
}

static void get_tile_body_bounds(const Game *g, int col, int row, float *l, float *t, float *r, float *b) {
    get_tile_bounds_px(col, row, l, t, r, b);
    *t = get_tile_feet_y(g, col, row);
}

static void get_tile_collision_bounds(const Game *g, int col, int row, bool use_surface_top,
                                      float *l, float *t, float *r, float *b) {
    if (use_surface_top)
        get_tile_body_bounds(g, col, row, l, t, r, b);
    else
        get_tile_bounds_px(col, row, l, t, r, b);
    *l += COLLISION_SKIN;
    *r -= COLLISION_SKIN;
}

static bool player_hits_tile_ceiling(float px, float py, float t_l, float t_r, float t_b) {
    if (px < t_l || px > t_r)
        return false;
    return (py - PLAYER_HALF) < t_b;
}

static bool player_standing_on_tile(const Game *g, int col, int row,
                                    float p_l, float p_r, float p_b) {
    if (!tile_is_solid(get_tile_at(g, col, row)))
        return false;

    float surf = get_tile_feet_y(g, col, row);
    if (p_b < surf - GROUNDED_EPSILON || p_b > surf + GROUNDED_EPSILON)
        return false;

    float t_l, t_t, t_r, t_b;
    get_tile_bounds_px(col, row, &t_l, &t_t, &t_r, &t_b);
    return p_l < t_r && p_r > t_l;
}

static bool game_init_render_target(Game *g) {
    if (g->target_loaded)
        return true;

    g->target = LoadRenderTexture(VIEW_WIDTH, VIEW_HEIGHT);
    SetTextureFilter(g->target.texture, TEXTURE_FILTER_POINT);
    g->target_loaded = (g->target.texture.id != 0);
    return g->target_loaded;
}

static void game_clamp_player_to_room(Game *g) {
    const RoomDef *room = level_get_active_room(&g->level);
    if (!room)
        return;

    float hw = PLAYER_HALF;
    float min_x = room->x + hw;
    float max_x = room->x + room->w - hw;
    if (g->player.pos.x < min_x)
        g->player.pos.x = min_x;
    if (g->player.pos.x > max_x)
        g->player.pos.x = max_x;

    float min_y = room->y + hw;
    float max_y = room->y + room->h - hw;
    if (g->player.pos.y < min_y)
        g->player.pos.y = min_y;
    if (g->player.pos.y > max_y)
        g->player.pos.y = max_y;
}

static bool check_grounded(const Game *g, float px, float py);

static void game_sync_trigger_overlap_state(Game *g) {
    g->trigger_overlap_tunnel = NULL;
    g->trigger_overlap_teleport = NULL;

    const Level *level = &g->level;
    const RoomDef *room = level_get_active_room(level);
    if (!room)
        return;

    float pl, pt, pr, pb;
    get_player_aabb(g->player.pos.x, g->player.pos.y, &pl, &pt, &pr, &pb);

    if (!room->isolated && level->tunnels) {
        for (int i = 0; i < level->tunnel_count; i++) {
            const TunnelDef *tunnel = &level->tunnels[i];
            if (!level_tunnel_other_room(tunnel, room->id))
                continue;
            if (aabb_overlap(pl, pt, pr, pb, tunnel->x, tunnel->y,
                             tunnel->x + tunnel->w, tunnel->y + tunnel->h)) {
                g->trigger_overlap_tunnel = tunnel;
                break;
            }
        }
    }

    if (level->teleports) {
        for (int i = 0; i < level->teleport_count; i++) {
            const TeleportDef *tp = &level->teleports[i];
            if (!tp->room_id || strcmp(tp->room_id, room->id) != 0)
                continue;
            if (aabb_overlap(pl, pt, pr, pb, tp->x, tp->y, tp->x + tp->w, tp->y + tp->h)) {
                g->trigger_overlap_teleport = tp;
                break;
            }
        }
    }
}

static void game_clear_pending_transition(Game *g) {
    g->pending_kind = PENDING_NONE;
    g->pending_tunnel = NULL;
    g->pending_target_room_id = NULL;
    g->pending_from_teleport = NULL;
    g->pending_to_teleport = NULL;
}

static void game_begin_tunnel_crossing(Game *g, const TunnelDef *tunnel,
                                       const char *target_room_id) {
    Vector2 spawn_pos;
    int room_index = level_find_room_index_by_id(&g->level, target_room_id);

    g->player_pan_from_x = g->player.pos.x;
    g->player_pan_from_y = g->player.pos.y;

    if (room_index >= 0)
        g->level.active_room_index = room_index;
    level_place_at_tunnel(&g->level, tunnel, NULL, target_room_id, &spawn_pos);
    g->player_pan_to_x = spawn_pos.x;
    g->player_pan_to_y = spawn_pos.y;

    g->player.vel = (Vector2){ 0.0f, 0.0f };
    g->player.grounded = true;
    g->player.on_wall_left = false;
    g->player.on_wall_right = false;

    g->camera_pan_t = 0.0f;
    g->transition = TRANS_CAMERA_PAN;
}

static void game_complete_transition(Game *g) {
    Vector2 pos;

    if (g->pending_kind == PENDING_TELEPORT && g->pending_to_teleport) {
        const TeleportDef *to = g->pending_to_teleport;
        if (to->room_id) {
            int room_index = level_find_room_index_by_id(&g->level, to->room_id);
            if (room_index >= 0)
                g->level.active_room_index = room_index;
        }
        level_place_at_teleport(&g->level, to, &pos);
        g->player.pos = pos;
    } else {
        game_clear_pending_transition(g);
        return;
    }

    g->player.vel = (Vector2){ 0.0f, 0.0f };
    g->player.grounded = true;
    g->player.on_wall_left = false;
    g->player.on_wall_right = false;

    game_clear_pending_transition(g);
    game_sync_trigger_overlap_state(g);
    game_clamp_camera(g);
}

static void game_update_transition(Game *g, float dt) {
    if (g->transition == TRANS_CAMERA_PAN) {
        g->camera_pan_t += dt / TUNNEL_PAN_DURATION;
        if (g->camera_pan_t >= 1.0f) {
            g->camera_pan_t = 1.0f;
            g->player.pos.x = g->player_pan_to_x;
            g->player.pos.y = g->player_pan_to_y;
            g->transition = TRANS_NONE;
            game_sync_trigger_overlap_state(g);
        }
        float t = smoothstep(g->camera_pan_t);
        g->player.pos.x = g->player_pan_from_x + (g->player_pan_to_x - g->player_pan_from_x) * t;
        g->player.pos.y = g->player_pan_from_y + (g->player_pan_to_y - g->player_pan_from_y) * t;
        game_clamp_camera_to_player(g);
    } else if (g->transition == TRANS_FADE_OUT) {
        g->transition_alpha += TRANS_FADE_SPEED * dt;
        if (g->transition_alpha >= 255.0f) {
            g->transition_alpha = 255.0f;
            game_complete_transition(g);
            g->transition = TRANS_FADE_IN;
        }
    } else if (g->transition == TRANS_FADE_IN) {
        g->transition_alpha -= TRANS_FADE_SPEED * dt;
        if (g->transition_alpha <= 0.0f) {
            g->transition_alpha = 0.0f;
            g->transition = TRANS_NONE;
        }
    }
}

static void game_check_transition_triggers(Game *g) {
    if (g->transition != TRANS_NONE)
        return;

    const Level *level = &g->level;
    const RoomDef *room = level_get_active_room(level);
    if (!room)
        return;

    float pl, pt, pr, pb;
    get_player_aabb(g->player.pos.x, g->player.pos.y, &pl, &pt, &pr, &pb);

    const TunnelDef *tunnel_overlap = NULL;
    const TeleportDef *teleport_overlap = NULL;

    if (!room->isolated && level->tunnels) {
        for (int i = 0; i < level->tunnel_count; i++) {
            const TunnelDef *tunnel = &level->tunnels[i];
            const char *target_room = level_tunnel_other_room(tunnel, room->id);
            if (!target_room)
                continue;

            if (!aabb_overlap(pl, pt, pr, pb, tunnel->x, tunnel->y,
                              tunnel->x + tunnel->w, tunnel->y + tunnel->h))
                continue;

            tunnel_overlap = tunnel;
            if (tunnel != g->trigger_overlap_tunnel)
                game_begin_tunnel_crossing(g, tunnel, target_room);
            break;
        }
    }

    if (g->transition == TRANS_NONE && level->teleports) {
        for (int i = 0; i < level->teleport_count; i++) {
            const TeleportDef *tp = &level->teleports[i];
            if (!tp->room_id || strcmp(tp->room_id, room->id) != 0)
                continue;
            if (!tp->link_id)
                continue;

            if (!aabb_overlap(pl, pt, pr, pb, tp->x, tp->y, tp->x + tp->w, tp->y + tp->h))
                continue;

            teleport_overlap = tp;
            if (tp != g->trigger_overlap_teleport) {
                const TeleportDef *link = level_find_teleport_by_id(level, tp->link_id);
                if (!link)
                    break;

                g->pending_kind = PENDING_TELEPORT;
                g->pending_from_teleport = tp;
                g->pending_to_teleport = link;
                g->transition = TRANS_FADE_OUT;
                g->transition_alpha = 0.0f;
            }
            break;
        }
    }

    g->trigger_overlap_tunnel = tunnel_overlap;
    g->trigger_overlap_teleport = teleport_overlap;
}

static void game_spawn_at_save(Game *g, int save_index) {
    if (!level_set_active_save(&g->level, save_index))
        return;

    g->debug_save_index = save_index;
    g->player.spawn = g->level.spawn;
    g->player.pos = g->player.spawn;
    g->player.vel = (Vector2){ 0.0f, 0.0f };
    g->player.grounded = false;
    g->player.on_wall_left = false;
    g->player.on_wall_right = false;
    g->player.air_jumps_left = 1;
    g->player.coyote_timer = COYOTE_TIME;
    g->player.jump_buffer_timer = 0.0f;
    game_clamp_camera(g);
    game_sync_trigger_overlap_state(g);
}

static void game_reset_player(Game *g) {
    g->player.spawn = g->level.spawn;
    g->player.pos = g->player.spawn;
    g->player.vel = (Vector2){ 0.0f, 0.0f };
    g->player.grounded = false;
    g->player.on_wall_left = false;
    g->player.on_wall_right = false;
    g->player.air_jumps_left = 1;
    g->player.coyote_timer = COYOTE_TIME;
    g->player.jump_buffer_timer = 0.0f;
    game_sync_trigger_overlap_state(g);
}

static bool game_load_act(Game *g, int index) {
    if (index < 0 || index >= ACT_COUNT)
        return false;

    level_free(&g->level);
    if (!level_load(&g->level, &ACTS[index]))
        return false;

    g->active_act_index = index;
    g->map_open = false;
    g->save_menu_open = false;
    g->debug_save_index = 0;
    g->transition = TRANS_NONE;
    g->transition_alpha = 0.0f;
    game_clear_pending_transition(g);
    game_reset_player(g);
    game_clamp_camera(g);
    return true;
}

static void resolve_collision_horizontal(float *px, float *vx,
                                       float p_l, float p_t, float p_r, float p_b,
                                       float t_l, float t_t, float t_r, float t_b) {
    float pen_l = p_r - t_l;
    float pen_r = t_r - p_l;
    float pen_t = p_b - t_t;
    float pen_b = t_b - p_t;

    float min_h = fminf(pen_l, pen_r);
    float min_v = fminf(pen_t, pen_b);

    if (!(min_h < min_v))
        return;

    if (pen_l < pen_r) {
        *px -= pen_l;
        *vx = 0.0f;
    } else {
        *px += pen_r;
        *vx = 0.0f;
    }
}

static bool resolve_collision_vertical(float *py, float *vy,
                                       float p_l, float p_t, float p_r, float p_b,
                                       float t_l, float t_t, float t_r, float t_b) {
    float pen_l = p_r - t_l;
    float pen_r = t_r - p_l;
    float pen_t = p_b - t_t;
    float pen_b = t_b - p_t;

    float min_h = fminf(pen_l, pen_r);
    float min_v = fminf(pen_t, pen_b);

    if (!(min_v <= min_h))
        return false;

    float vy_in = *vy;
    if (pen_t < pen_b) {
        *py -= pen_t;
        *vy = 0.0f;
    } else {
        *py += pen_b;
        *vy = 0.0f;
    }

    if (vy_in >= 0.0f && p_b > t_t && p_t < t_t + (float)TILE_SIZE * 0.5f)
        return true;
    return false;
}

static bool check_grounded(const Game *g, float px, float py) {
    float pl, pt, pr, pb;
    get_player_aabb(px, py, &pl, &pt, &pr, &pb);

    int feet_row = (int)floorf(pb / (float)TILE_SIZE);
    int c0 = (int)floorf(pl / (float)TILE_SIZE);
    int c1 = (int)floorf(pr / (float)TILE_SIZE);

    for (int c = c0; c <= c1; c++) {
        if (!tile_is_solid(get_tile_at(g, c, feet_row)))
            continue;
        float tile_top = get_tile_feet_y(g, c, feet_row);
        float dist = pb - tile_top;
        if (dist >= 0.0f && dist <= GROUNDED_EPSILON)
            return true;
    }
    return false;
}

static bool resolve_collisions(Game *g, float *px, float *py, float *vx, float *vy, float dt,
                               bool *out_wall_left, bool *out_wall_right) {
    const int max_iter = 4;
    bool wall_left = false;
    bool wall_right = false;

    *px += *vx * dt;

    for (int iter = 0; iter < max_iter; iter++) {
        float pl, pt, pr, pb;
        get_player_aabb(*px, *py, &pl, &pt, &pr, &pb);
        get_player_aabb_horizontal(*px, *py, &pl, &pt, &pr, &pb);
        int c0 = (int)floorf(pl / (float)TILE_SIZE) - 1;
        int c1 = (int)floorf(pr / (float)TILE_SIZE) + 1;
        int r0 = (int)floorf(pt / (float)TILE_SIZE) - 1;
        int r1 = (int)floorf(pb / (float)TILE_SIZE) + 1;

        float px_before = *px;
        float feet_l, feet_t, feet_r, feet_b;
        get_player_aabb(*px, *py, &feet_l, &feet_t, &feet_r, &feet_b);

        for (int r = r0; r <= r1; r++) {
            for (int c = c0; c <= c1; c++) {
                if (!tile_is_solid(get_tile_at(g, c, r)))
                    continue;

                if (player_standing_on_tile(g, c, r, feet_l, feet_r, feet_b))
                    continue;

                float tl, tt, tr, tb;
                get_tile_collision_bounds(g, c, r, true, &tl, &tt, &tr, &tb);

                if (!aabb_overlap(pl, pt, pr, pb, tl, tt, tr, tb))
                    continue;

                resolve_collision_horizontal(px, vx, pl, pt, pr, pb, tl, tt, tr, tb);
            }
        }
        if (*px < px_before - 0.01f)
            wall_left = true;
        if (*px > px_before + 0.01f)
            wall_right = true;
    }

    *py += *vy * dt;

    bool grounded = false;
    for (int iter = 0; iter < max_iter; iter++) {
        float pl, pt, pr, pb;
        get_player_aabb(*px, *py, &pl, &pt, &pr, &pb);
        int c0 = (int)floorf(pl / (float)TILE_SIZE) - 1;
        int c1 = (int)floorf(pr / (float)TILE_SIZE) + 1;
        int r0 = (int)floorf(pt / (float)TILE_SIZE) - 1;
        int r1 = (int)floorf(pb / (float)TILE_SIZE) + 1;

        for (int r = r0; r <= r1; r++) {
            for (int c = c0; c <= c1; c++) {
                if (!tile_is_solid(get_tile_at(g, c, r)))
                    continue;

                float tl, tt, tr, tb;
                get_tile_collision_bounds(g, c, r, true, &tl, &tt, &tr, &tb);
                get_player_aabb(*px, *py, &pl, &pt, &pr, &pb);

                if (!aabb_overlap(pl, pt, pr, pb, tl, tt, tr, tb))
                    continue;

                if (*vy < 0.0f && !player_hits_tile_ceiling(*px, *py, tl, tr, tb))
                    continue;

                if (resolve_collision_vertical(py, vy, pl, pt, pr, pb, tl, tt, tr, tb))
                    grounded = true;
            }
        }
    }

    if (!grounded)
        grounded = check_grounded(g, *px, *py);

    *out_wall_left = wall_left;
    *out_wall_right = wall_right;
    return grounded;
}

static bool jump_pressed(void) {
    return IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
}

static void try_jump(Player *p) {
    bool can_ground_jump = p->grounded || p->coyote_timer > 0.0f;
    bool can_air_jump = !p->grounded && p->coyote_timer <= 0.0f && p->air_jumps_left > 0;

    if (can_ground_jump) {
        p->vel.y = JUMP_VELOCITY;
        p->grounded = false;
        p->coyote_timer = 0.0f;
        p->jump_buffer_timer = 0.0f;
        p->air_jumps_left = 1;
        return;
    }

    if (can_air_jump) {
        p->vel.y = JUMP_VELOCITY;
        p->air_jumps_left--;
        p->jump_buffer_timer = 0.0f;
    }
}

static int ui_act_menu_height(const Game *g) {
    int rows = g->act_menu_open ? (ACT_COUNT + 1) : 1;
    return UI_ACT_MENU_PAD * 2 + rows * UI_ACT_MENU_ROW_H;
}

static int ui_save_menu_y(const Game *g) {
    return UI_ACT_MENU_Y + ui_act_menu_height(g) + UI_SAVE_MENU_GAP;
}

static int ui_save_menu_height(const Game *g) {
    if (!g->debug_mode || g->level.save_count <= 0)
        return 0;
    int rows = g->save_menu_open ? (g->level.save_count + 1) : 1;
    return UI_ACT_MENU_PAD * 2 + rows * UI_ACT_MENU_ROW_H;
}

typedef struct {
    int draw_x;
    int draw_y;
    int draw_w;
    int draw_h;
    float scale;
} GameViewport;

static GameViewport game_viewport(const Game *g) {
    GameViewport vp = { 0 };
    (void)g;
    vp.scale = (float)WINDOW_SCALE;
    vp.draw_w = WINDOW_WIDTH;
    vp.draw_h = WINDOW_HEIGHT;
    vp.draw_x = 0;
    vp.draw_y = 0;
    return vp;
}

static Rectangle ui_debug_button_rect(void) {
    int win_w = GetScreenWidth();
    return (Rectangle){
        (float)(win_w - UI_DEBUG_BTN_W - UI_DEBUG_BTN_PAD),
        (float)UI_DEBUG_BTN_PAD,
        (float)UI_DEBUG_BTN_W,
        (float)UI_DEBUG_BTN_H
    };
}

static Rectangle ui_map_button_rect(void) {
    return (Rectangle){
        (float)(UI_ACT_MENU_X + UI_ACT_MENU_W + UI_MAP_BTN_GAP),
        (float)UI_ACT_MENU_Y,
        (float)UI_MAP_BTN_W,
        (float)UI_MAP_BTN_H
    };
}

typedef struct {
    Rectangle panel;
    Rectangle content;
    float scale;
} MapLayout;

static MapLayout game_map_layout(const Game *g) {
    const Level *level = &g->level;
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    MapLayout layout = { 0 };

    if (level->width <= 0 || level->height <= 0)
        return layout;

    float panel_w = (float)sw * UI_MAP_SCREEN_FRACTION;
    float panel_h = (float)sh * UI_MAP_SCREEN_FRACTION;
    float panel_x = ((float)sw - panel_w) * 0.5f;
    float panel_y = ((float)sh - panel_h) * 0.5f;

    float inner_w = panel_w - (float)UI_MAP_PANEL_PAD * 2.0f;
    float inner_h = panel_h - (float)UI_MAP_TITLE_H - (float)UI_MAP_PANEL_PAD * 2.0f;
    float aspect = (float)level->width / (float)level->height;

    float content_w;
    float content_h;
    if (inner_w / inner_h > aspect) {
        content_h = inner_h;
        content_w = content_h * aspect;
    } else {
        content_w = inner_w;
        content_h = content_w / aspect;
    }

    layout.panel = (Rectangle){ panel_x, panel_y, panel_w, panel_h };
    layout.content = (Rectangle){
        panel_x + (panel_w - content_w) * 0.5f,
        panel_y + (float)UI_MAP_TITLE_H + (float)UI_MAP_PANEL_PAD
            + (inner_h - content_h) * 0.5f,
        content_w,
        content_h
    };
    layout.scale = content_w / (float)level->width;
    return layout;
}

static Vector2 map_world_to_screen(float wx, float wy, const MapLayout *layout) {
    return (Vector2){
        layout->content.x + wx * layout->scale,
        layout->content.y + wy * layout->scale
    };
}

static bool ui_point_in_save_menu(const Game *g, Vector2 mouse) {
    if (!g->debug_mode || g->level.save_count <= 0)
        return false;
    Rectangle menu = {
        (float)UI_ACT_MENU_X,
        (float)ui_save_menu_y(g),
        (float)UI_ACT_MENU_W,
        (float)ui_save_menu_height(g)
    };
    return CheckCollisionPointRec(mouse, menu);
}

static bool ui_point_in_act_menu(const Game *g, Vector2 mouse) {
    Rectangle menu = {
        (float)UI_ACT_MENU_X,
        (float)UI_ACT_MENU_Y,
        (float)UI_ACT_MENU_W,
        (float)ui_act_menu_height(g)
    };
    return CheckCollisionPointRec(mouse, menu);
}

static void game_handle_ui_input(Game *g) {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        return;

    Vector2 mouse = GetMousePosition();
    Rectangle debug_btn = ui_debug_button_rect();
    Rectangle map_btn = ui_map_button_rect();

    if (CheckCollisionPointRec(mouse, debug_btn)) {
        g->debug_mode = !g->debug_mode;
        g->act_menu_open = false;
        g->save_menu_open = false;
        return;
    }

    if (g->debug_mode && ui_point_in_save_menu(g, mouse)) {
        if (g->save_menu_open) {
            int row = (int)((mouse.y - (float)ui_save_menu_y(g) - (float)UI_ACT_MENU_PAD) /
                            (float)UI_ACT_MENU_ROW_H);
            if (row == 0) {
                g->save_menu_open = false;
                return;
            }
            int save_row = row - 1;
            if (save_row >= 0 && save_row < g->level.save_count) {
                int save_index = g->level.saves[save_row].index;
                g->save_menu_open = false;
                if (save_index != g->debug_save_index)
                    game_spawn_at_save(g, save_index);
            }
        } else {
            g->save_menu_open = true;
            g->act_menu_open = false;
        }
        return;
    }

    if (CheckCollisionPointRec(mouse, map_btn)) {
        g->map_open = !g->map_open;
        g->act_menu_open = false;
        g->save_menu_open = false;
        return;
    }

    if (ui_point_in_act_menu(g, mouse)) {
        if (g->act_menu_open) {
            int row = (int)((mouse.y - (float)UI_ACT_MENU_Y - (float)UI_ACT_MENU_PAD) /
                            (float)UI_ACT_MENU_ROW_H);
            if (row == 0) {
                g->act_menu_open = false;
                return;
            }
            int act_index = row - 1;
            g->act_menu_open = false;
            g->save_menu_open = false;
            if (act_index >= 0 && act_index < ACT_COUNT && act_index != g->active_act_index)
                game_load_act(g, act_index);
        } else {
            g->act_menu_open = true;
            g->save_menu_open = false;
        }
        return;
    }

    if (g->map_open) {
        MapLayout layout = game_map_layout(g);
        if (!CheckCollisionPointRec(mouse, layout.panel)) {
            g->map_open = false;
            return;
        }
    }

    if (g->act_menu_open)
        g->act_menu_open = false;
    if (g->save_menu_open)
        g->save_menu_open = false;
}

static void game_handle_input(Game *g, float dt) {
    game_handle_ui_input(g);

    if (g->transition != TRANS_NONE)
        return;

    if (IsKeyPressed(KEY_M)) {
        g->map_open = !g->map_open;
        if (g->map_open)
            g->act_menu_open = false;
    }

    if (g->map_open)
        return;

    Player *p = &g->player;

    if (IsKeyPressed(KEY_R))
        p->pos = p->spawn;

    float move_input = 0.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
        move_input -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
        move_input += 1.0f;

    float accel = p->grounded ? MOVE_SPEED : AIR_MOVE_SPEED;
    p->vel.x += move_input * accel * dt;

    if (fabsf(p->vel.x) > MAX_RUN_SPEED)
        p->vel.x = copysignf(MAX_RUN_SPEED, p->vel.x);

    if (move_input == 0.0f && p->grounded) {
        float friction = MOVE_SPEED * 3.0f * dt;
        if (fabsf(p->vel.x) <= friction)
            p->vel.x = 0.0f;
        else
            p->vel.x -= copysignf(friction, p->vel.x);
    }

    if (jump_pressed()) {
        p->jump_buffer_timer = JUMP_BUFFER_TIME;
        try_jump(p);
    }
}

static void game_update(Game *g, float dt) {
    game_update_transition(g, dt);

    if (g->transition == TRANS_NONE && !g->map_open) {
        Player *p = &g->player;
        bool was_grounded = p->grounded;

        if (!p->grounded)
            p->vel.y += GRAVITY * dt;

        bool wall_left = false;
        bool wall_right = false;
        p->grounded = resolve_collisions(g, &p->pos.x, &p->pos.y, &p->vel.x, &p->vel.y, dt,
                                         &wall_left, &wall_right);
        p->on_wall_left = wall_left;
        p->on_wall_right = wall_right;

        if (p->grounded) {
            p->coyote_timer = COYOTE_TIME;
            p->air_jumps_left = 1;
            if (p->jump_buffer_timer > 0.0f)
                try_jump(p);
        } else if (was_grounded) {
            p->coyote_timer = COYOTE_TIME;
        } else if (p->coyote_timer > 0.0f) {
            p->coyote_timer -= dt;
            if (p->coyote_timer < 0.0f)
                p->coyote_timer = 0.0f;
        }

        if (!p->grounded && (p->on_wall_left || p->on_wall_right) && p->vel.y > WALL_SLIDE_MAX_VY)
            p->vel.y = WALL_SLIDE_MAX_VY;

        if (p->jump_buffer_timer > 0.0f) {
            p->jump_buffer_timer -= dt;
            if (p->jump_buffer_timer < 0.0f)
                p->jump_buffer_timer = 0.0f;
        }

        game_clamp_player_to_room(g);

        if (p->pos.y < PLAYER_HALF)
            p->pos.y = PLAYER_HALF;

        game_check_transition_triggers(g);
    }

    game_clamp_camera(g);
}

static void game_draw_layer_slice(Texture2D tex, float src_x, float src_y, float view_w, float view_h) {
    if (tex.id == 0)
        return;

    Rectangle src = { src_x, src_y, view_w, view_h };
    Rectangle dst = { 0.0f, 0.0f, view_w, view_h };
    DrawTexturePro(tex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
}

static void game_draw_debug_collision(const Game *g, float view_w, float view_h) {
    const Level *level = &g->level;
    int c0 = (int)floorf(g->camera.x / (float)TILE_SIZE) - 1;
    int c1 = (int)floorf((g->camera.x + view_w) / (float)TILE_SIZE) + 1;
    int r0 = (int)floorf(g->camera.y / (float)TILE_SIZE) - 1;
    int r1 = (int)floorf((g->camera.y + view_h) / (float)TILE_SIZE) + 1;

    if (c0 < 0)
        c0 = 0;
    if (c1 >= level->cols)
        c1 = level->cols - 1;
    if (r0 < 0)
        r0 = 0;
    if (r1 >= level->rows)
        r1 = level->rows - 1;

    for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
            if (!tile_is_solid(level_get_tile(level, c, r)))
                continue;

            float tl, tt_full, tr, tb;
            get_tile_bounds_px(c, r, &tl, &tt_full, &tr, &tb);
            float surf = get_tile_feet_y(g, c, r);

            DrawRectangleLinesEx((Rectangle){ tl, tt_full, tr - tl, tb - tt_full }, 1.0f,
                                 (Color){ 100, 100, 100, 120 });

            float body_l, body_t, body_r, body_b;
            get_tile_collision_bounds(g, c, r, true, &body_l, &body_t, &body_r, &body_b);
            if (body_r > body_l) {
                DrawRectangleLinesEx((Rectangle){ body_l, body_t, body_r - body_l, body_b - body_t },
                                     1.0f, (Color){ 50, 255, 100, 220 });
            }

            DrawLineV((Vector2){ body_l, surf }, (Vector2){ body_r, surf },
                      (Color){ 0, 255, 255, 200 });
        }
    }

    float pl, pt, pr, pb;
    get_player_aabb(g->player.pos.x, g->player.pos.y, &pl, &pt, &pr, &pb);
    DrawRectangleLinesEx((Rectangle){ pl, pt, pr - pl, pb - pt }, 1.0f, RED);

    get_player_aabb_horizontal(g->player.pos.x, g->player.pos.y, &pl, &pt, &pr, &pb);
    DrawRectangleLinesEx((Rectangle){ pl, pt, pr - pl, pb - pt }, 1.0f,
                         (Color){ 255, 165, 0, 255 });
}

static const char *game_room_label(const RoomDef *room) {
    if (room->name && room->name[0])
        return room->name;
    return room->id;
}

static void game_draw_spawn_marker(Vector2 pos, float radius, Color color) {
    DrawCircleV(pos, radius, color);
    DrawCircleLinesV(pos, radius + 1.0f, RAYWHITE);
    DrawLine((int)(pos.x - 8.0f), (int)pos.y, (int)(pos.x + 8.0f), (int)pos.y, color);
    DrawLine((int)pos.x, (int)(pos.y - 8.0f), (int)pos.x, (int)(pos.y + 8.0f), color);
}

static void game_draw_transition_spawn_dots(const Level *level) {
    if (!level)
        return;

    if (level->tunnels) {
        for (int i = 0; i < level->tunnel_count; i++) {
            const TunnelDef *tunnel = &level->tunnels[i];
            DrawCircleV((Vector2){ tunnel->spawn_ax, tunnel->spawn_ay }, 3.0f, BLACK);
            DrawCircleV((Vector2){ tunnel->spawn_bx, tunnel->spawn_by }, 3.0f, BLACK);
        }
    }

    if (level->teleports) {
        for (int i = 0; i < level->teleport_count; i++) {
            const TeleportDef *tp = &level->teleports[i];
            DrawCircleV((Vector2){ tp->spawn_x, tp->spawn_y }, 3.0f, BLACK);
        }
    }
}

static void game_draw_map_transition_spawn_dots(const Level *level, const MapLayout *layout) {
    if (!level || !layout)
        return;

    if (level->tunnels) {
        for (int i = 0; i < level->tunnel_count; i++) {
            const TunnelDef *tunnel = &level->tunnels[i];
            DrawCircleV(map_world_to_screen(tunnel->spawn_ax, tunnel->spawn_ay, layout), 3.0f, BLACK);
            DrawCircleV(map_world_to_screen(tunnel->spawn_bx, tunnel->spawn_by, layout), 3.0f, BLACK);
        }
    }

    if (level->teleports) {
        for (int i = 0; i < level->teleport_count; i++) {
            const TeleportDef *tp = &level->teleports[i];
            DrawCircleV(map_world_to_screen(tp->spawn_x, tp->spawn_y, layout), 3.0f, BLACK);
        }
    }
}

static void game_draw_debug_spawn(const Game *g) {
    const Level *level = &g->level;
    Color active_color = (Color){ 255, 140, 0, 255 };
    Color idle_color = (Color){ 255, 220, 80, 200 };

    for (int i = 0; i < level->save_count; i++) {
        const SavePointDef *save = &level->saves[i];
        bool active = save->index == g->debug_save_index;
        Color color = active ? active_color : idle_color;
        Vector2 pos = { save->x, save->y };
        game_draw_spawn_marker(pos, active ? 6.0f : 4.0f, color);
        char label[24];
        snprintf(label, sizeof(label), "save-%d", save->index);
        DrawText(label, (int)(pos.x + 8.0f), (int)(pos.y - 6.0f), 10, color);
    }
}

static void game_draw_debug_rooms(const Game *g) {
    const Level *level = &g->level;
    if (!level->rooms)
        return;

    for (int i = 0; i < level->room_count; i++) {
        const RoomDef *room = &level->rooms[i];
        Color fill = (i == level->active_room_index)
            ? (Color){ 80, 160, 255, 80 }
            : (Color){ 80, 80, 200, 50 };
        if (room->isolated)
            fill = (i == level->active_room_index)
                ? (Color){ 160, 80, 200, 80 }
                : (Color){ 120, 60, 160, 50 };

        DrawRectangleRec((Rectangle){ room->x, room->y, room->w, room->h }, fill);
        DrawRectangleLinesEx((Rectangle){ room->x, room->y, room->w, room->h }, 1.0f,
                             room->isolated
                                 ? (Color){ 200, 120, 255, 200 }
                                 : (Color){ 120, 180, 255, 200 });
    }

    if (level->tunnels) {
        for (int i = 0; i < level->tunnel_count; i++) {
            const TunnelDef *tunnel = &level->tunnels[i];
            DrawRectangleRec((Rectangle){ tunnel->x, tunnel->y, tunnel->w, tunnel->h },
                             (Color){ 255, 255, 100, 100 });
            DrawRectangleLinesEx((Rectangle){ tunnel->x, tunnel->y, tunnel->w, tunnel->h }, 1.0f,
                                 (Color){ 255, 255, 0, 220 });
        }
    }

    if (level->teleports) {
        for (int i = 0; i < level->teleport_count; i++) {
            const TeleportDef *tp = &level->teleports[i];
            DrawRectangleRec((Rectangle){ tp->x, tp->y, tp->w, tp->h },
                             (Color){ 255, 100, 255, 120 });
            DrawRectangleLinesEx((Rectangle){ tp->x, tp->y, tp->w, tp->h }, 1.0f,
                                 (Color){ 255, 0, 255, 220 });
        }
    }

    game_draw_debug_spawn(g);
    game_draw_transition_spawn_dots(level);
}

static void game_draw_map_collision_debug(const Game *g, const MapLayout *layout) {
    const Level *level = &g->level;
    Color collision_fill = (Color){ 50, 255, 100, 90 };
    Color collision_outline = (Color){ 50, 255, 100, 180 };

    for (int r = 0; r < level->rows; r++) {
        for (int c = 0; c < level->cols; c++) {
            if (!tile_is_solid(level_get_tile(level, c, r)))
                continue;

            float wx = (float)(c * TILE_SIZE);
            float wy = (float)(r * TILE_SIZE);
            float x = layout->content.x + wx * layout->scale;
            float y = layout->content.y + wy * layout->scale;
            float w = (float)TILE_SIZE * layout->scale;
            float h = (float)TILE_SIZE * layout->scale;
            if (w < 1.0f)
                w = 1.0f;
            if (h < 1.0f)
                h = 1.0f;

            Rectangle cell = { x, y, w, h };
            DrawRectangleRec(cell, collision_fill);
            DrawRectangleLinesEx(cell, 1.0f, collision_outline);
        }
    }
}

static void game_draw_map_spawn_debug(const Game *g, const MapLayout *layout) {
    const Level *level = &g->level;
    Color active_color = (Color){ 255, 140, 0, 255 };
    Color idle_color = (Color){ 255, 220, 80, 200 };

    for (int i = 0; i < level->save_count; i++) {
        const SavePointDef *save = &level->saves[i];
        bool active = save->index == g->debug_save_index;
        Color color = active ? active_color : idle_color;
        Vector2 screen = map_world_to_screen(save->x, save->y, layout);
        DrawCircleV(screen, active ? 5.0f : 4.0f, color);
        DrawCircleLinesV(screen, active ? 6.0f : 5.0f, RAYWHITE);
        char label[24];
        snprintf(label, sizeof(label), "save-%d", save->index);
        DrawText(label, (int)(screen.x + 8.0f), (int)(screen.y - 6.0f), 10, color);
    }

    game_draw_map_transition_spawn_dots(level, layout);
}

static void game_draw_world(Game *g) {
    BeginTextureMode(g->target);
    ClearBackground((Color){ 135, 206, 235, 255 });

    float view_w = (float)VIEW_WIDTH;
    float view_h = (float)VIEW_HEIGHT;

    game_draw_layer_slice(g->level.tex_background, g->camera.x * PARALLAX_FACTOR, g->camera.y,
                          view_w, view_h);

    Camera2D cam = { 0 };
    cam.target = (Vector2){ g->camera.x + view_w * 0.5f, g->camera.y + view_h * 0.5f };
    cam.offset = (Vector2){ view_w * 0.5f, view_h * 0.5f };
    cam.rotation = 0.0f;
    cam.zoom = 1.0f;

    BeginMode2D(cam);
    if (g->level.tex_base.id != 0)
        DrawTexture(g->level.tex_base, 0, 0, WHITE);

    if (g->debug_mode) {
        game_draw_debug_collision(g, view_w, view_h);
        game_draw_debug_rooms(g);
    }

    float pl = g->player.pos.x - PLAYER_HALF;
    float pt = g->player.pos.y - PLAYER_HALF;
    DrawRectangle((int)pl, (int)pt, (int)PLAYER_SIZE, (int)PLAYER_SIZE, RED);
    EndMode2D();

    if (g->transition_alpha > 0.0f) {
        unsigned char alpha = (unsigned char)g->transition_alpha;
        if (alpha > 255)
            alpha = 255;
        DrawRectangle(0, 0, (int)view_w, (int)view_h, (Color){ 0, 0, 0, alpha });
    }

    EndTextureMode();
}

static void game_draw_act_menu(const Game *g) {
    const ActDef *active = &ACTS[g->active_act_index];
    int menu_h = ui_act_menu_height(g);

    Rectangle menu_bg = {
        (float)UI_ACT_MENU_X,
        (float)UI_ACT_MENU_Y,
        (float)UI_ACT_MENU_W,
        (float)menu_h
    };
    DrawRectangleRec(menu_bg, (Color){ 20, 20, 30, 220 });
    DrawRectangleLinesEx(menu_bg, 1.0f, RAYWHITE);

    const char *header = TextFormat("%s v", active->label);
    DrawText(header,
             UI_ACT_MENU_X + UI_ACT_MENU_PAD,
             UI_ACT_MENU_Y + UI_ACT_MENU_PAD,
             UI_FONT_SIZE, RAYWHITE);

    if (!g->act_menu_open)
        return;

    for (int i = 0; i < ACT_COUNT; i++) {
        Color color = (i == g->active_act_index) ? (Color){ 180, 220, 255, 255 } : LIGHTGRAY;
        DrawText(ACTS[i].label,
                 UI_ACT_MENU_X + UI_ACT_MENU_PAD,
                 UI_ACT_MENU_Y + UI_ACT_MENU_PAD + (i + 1) * UI_ACT_MENU_ROW_H,
                 UI_FONT_SIZE, color);
    }
}

static void game_draw_save_menu(const Game *g) {
    if (!g->debug_mode || g->level.save_count <= 0)
        return;

    int menu_y = ui_save_menu_y(g);
    int menu_h = ui_save_menu_height(g);
    Rectangle menu_bg = {
        (float)UI_ACT_MENU_X,
        (float)menu_y,
        (float)UI_ACT_MENU_W,
        (float)menu_h
    };
    DrawRectangleRec(menu_bg, (Color){ 20, 20, 30, 220 });
    DrawRectangleLinesEx(menu_bg, 1.0f, (Color){ 120, 200, 120, 255 });

    const char *header = TextFormat("save-%d v", g->debug_save_index);
    DrawText(header,
             UI_ACT_MENU_X + UI_ACT_MENU_PAD,
             menu_y + UI_ACT_MENU_PAD,
             UI_FONT_SIZE, (Color){ 120, 200, 120, 255 });

    if (!g->save_menu_open)
        return;

    for (int i = 0; i < g->level.save_count; i++) {
        int save_index = g->level.saves[i].index;
        Color color = (save_index == g->debug_save_index) ? (Color){ 180, 220, 255, 255 } : LIGHTGRAY;
        DrawText(TextFormat("save-%d", save_index),
                 UI_ACT_MENU_X + UI_ACT_MENU_PAD,
                 menu_y + UI_ACT_MENU_PAD + (i + 1) * UI_ACT_MENU_ROW_H,
                 UI_FONT_SIZE, color);
    }
}

static void game_draw_debug_button(const Game *g) {
    Rectangle btn = ui_debug_button_rect();
    Color fill = g->debug_mode ? (Color){ 60, 120, 60, 230 } : (Color){ 40, 40, 50, 220 };
    DrawRectangleRec(btn, fill);
    DrawRectangleLinesEx(btn, 1.0f, g->debug_mode ? LIME : RAYWHITE);
    const char *label = g->debug_mode ? "DEBUG" : "DEBUG";
    int tw = MeasureText(label, UI_FONT_SIZE);
    DrawText(label,
             (int)(btn.x + (btn.width - (float)tw) * 0.5f),
             (int)(btn.y + (btn.height - (float)UI_FONT_SIZE) * 0.5f),
             UI_FONT_SIZE, g->debug_mode ? LIME : RAYWHITE);
}

static void game_draw_map_button(const Game *g) {
    Rectangle btn = ui_map_button_rect();
    Color fill = g->map_open ? (Color){ 60, 120, 60, 230 } : (Color){ 40, 40, 50, 220 };
    DrawRectangleRec(btn, fill);
    DrawRectangleLinesEx(btn, 1.0f, g->map_open ? LIME : RAYWHITE);
    const char *label = "MAP";
    int tw = MeasureText(label, UI_FONT_SIZE);
    DrawText(label,
             (int)(btn.x + (btn.width - (float)tw) * 0.5f),
             (int)(btn.y + (btn.height - (float)UI_FONT_SIZE) * 0.5f),
             UI_FONT_SIZE, g->map_open ? LIME : RAYWHITE);
}

static void game_draw_map(const Game *g) {
    const Level *level = &g->level;
    if (!level->loaded || level->width <= 0 || level->height <= 0)
        return;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 150 });

    MapLayout layout = game_map_layout(g);
    DrawRectangleRec(layout.panel, (Color){ 20, 20, 30, 240 });
    DrawRectangleLinesEx(layout.panel, 1.0f, RAYWHITE);

    const ActDef *act = &ACTS[g->active_act_index];
    const char *title = TextFormat("Map — %s", act->label);
    DrawText(title,
             (int)(layout.panel.x + (float)UI_MAP_PANEL_PAD),
             (int)(layout.panel.y + 4),
             UI_FONT_SIZE, RAYWHITE);

    DrawRectangleRec(layout.content, (Color){ 30, 30, 45, 255 });
    DrawRectangleLinesEx(layout.content, 1.0f, (Color){ 80, 80, 100, 255 });

    if (g->debug_mode)
        game_draw_map_collision_debug(g, &layout);

    int current_room = level_find_room_at(level, g->player.pos.x, g->player.pos.y);
    if (current_room < 0)
        current_room = level->active_room_index;

    if (level->rooms) {
        for (int i = 0; i < level->room_count; i++) {
            const RoomDef *room = &level->rooms[i];
            float rx = layout.content.x + room->x * layout.scale;
            float ry = layout.content.y + room->y * layout.scale;
            float rw = room->w * layout.scale;
            float rh = room->h * layout.scale;

            Color fill = (i == current_room)
                ? (Color){ 80, 160, 255, 120 }
                : (Color){ 80, 80, 200, 70 };
            Color outline = (i == current_room)
                ? (Color){ 120, 180, 255, 255 }
                : (Color){ 120, 140, 220, 200 };
            if (room->isolated) {
                fill = (i == current_room)
                    ? (Color){ 160, 80, 200, 120 }
                    : (Color){ 120, 60, 160, 70 };
                outline = (i == current_room)
                    ? (Color){ 200, 120, 255, 255 }
                    : (Color){ 160, 100, 220, 200 };
            }

            DrawRectangleRec((Rectangle){ rx, ry, rw, rh }, fill);
            DrawRectangleLinesEx((Rectangle){ rx, ry, rw, rh }, 1.0f, outline);

            const char *label = game_room_label(room);
            int lw = MeasureText(label, 10);
            int lx = (int)(rx + (rw - (float)lw) * 0.5f);
            int ly = (int)(ry + (rh - 10.0f) * 0.5f);
            DrawText(label, lx, ly, 10, RAYWHITE);
        }
    }

    if (level->tunnels) {
        for (int i = 0; i < level->tunnel_count; i++) {
            const TunnelDef *tunnel = &level->tunnels[i];
            float dx = layout.content.x + tunnel->x * layout.scale;
            float dy = layout.content.y + tunnel->y * layout.scale;
            float dw = tunnel->w * layout.scale;
            float dh = tunnel->h * layout.scale;
            if (dw < UI_MAP_DOOR_MIN_PX)
                dw = UI_MAP_DOOR_MIN_PX;
            if (dh < UI_MAP_DOOR_MIN_PX)
                dh = UI_MAP_DOOR_MIN_PX;

            DrawRectangleRec((Rectangle){ dx, dy, dw, dh }, (Color){ 255, 255, 100, 180 });
            DrawRectangleLinesEx((Rectangle){ dx, dy, dw, dh }, 1.0f,
                                 (Color){ 255, 255, 0, 255 });
        }
    }

    if (level->teleports) {
        for (int i = 0; i < level->teleport_count; i++) {
            const TeleportDef *tp = &level->teleports[i];
            float tx = layout.content.x + tp->x * layout.scale;
            float ty = layout.content.y + tp->y * layout.scale;
            float tw = tp->w * layout.scale;
            float th = tp->h * layout.scale;
            if (tw < UI_MAP_DOOR_MIN_PX)
                tw = UI_MAP_DOOR_MIN_PX;
            if (th < UI_MAP_DOOR_MIN_PX)
                th = UI_MAP_DOOR_MIN_PX;

            DrawRectangleRec((Rectangle){ tx, ty, tw, th }, (Color){ 255, 100, 255, 180 });
            DrawRectangleLinesEx((Rectangle){ tx, ty, tw, th }, 1.0f,
                                 (Color){ 255, 0, 255, 255 });
        }
    }

    Vector2 player_screen = map_world_to_screen(g->player.pos.x, g->player.pos.y, &layout);
    DrawCircleV(player_screen, 4.0f, RED);
    DrawCircleLinesV(player_screen, 5.0f, RAYWHITE);

    if (g->debug_mode)
        game_draw_map_spawn_debug(g, &layout);
}

static void game_draw(Game *g) {
    GameViewport vp = game_viewport(g);

    BeginDrawing();
    ClearBackground(BLACK);

    Rectangle src = { 0.0f, 0.0f, (float)VIEW_WIDTH, -(float)VIEW_HEIGHT };
    Rectangle dst = {
        (float)vp.draw_x, (float)vp.draw_y, (float)vp.draw_w, (float)vp.draw_h
    };
    if (g->target_loaded && g->target.texture.id != 0)
        DrawTexturePro(g->target.texture, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);

    game_draw_act_menu(g);
    game_draw_save_menu(g);

    if (g->map_open)
        game_draw_map(g);

    game_draw_map_button(g);
    game_draw_debug_button(g);

    DrawText("A/D: move | Space: jump | R: respawn | M: map | Acts: top-left",
             10, GetScreenHeight() - 22, 12, DARKGRAY);
    if (g->debug_mode) {
        const char *legend = g->map_open
            ? "Debug map: green=collision | orange/yellow=saves | black=tunnel/teleport spawn"
            : "Debug: green=body | orange/yellow=saves | Spawn menu below acts";
        DrawText(legend, 10, GetScreenHeight() - 38, 12, (Color){ 120, 200, 120, 255 });
    }
    EndDrawing();
}

static int game_default_act_index(void) {
    for (int i = 0; i < ACT_COUNT; i++) {
        if (ACTS[i].id && strcmp(ACTS[i].id, DEFAULT_ACT_ID) == 0)
            return i;
    }
    return 0;
}

bool game_new(Game **out) {
    if (!out)
        return false;

    Game *g = calloc(1, sizeof(Game));
    if (!g)
        return false;

    int default_act = game_default_act_index();
    g->active_act_index = default_act;
    g->act_menu_open = false;
    g->save_menu_open = false;
    g->map_open = false;
    g->debug_mode = false;
    g->debug_save_index = 0;

    if (!game_load_act(g, default_act)) {
        free(g);
        return false;
    }

    if (!game_init_render_target(g)) {
        free(g);
        return false;
    }

    g->running = true;

    *out = g;
    return true;
}

void game_free(Game **game) {
    if (!game || !*game)
        return;

    Game *g = *game;
    if (g->target_loaded)
        UnloadRenderTexture(g->target);
    level_free(&g->level);
    free(g);
    *game = NULL;
}

bool game_run(Game *game) {
    if (!game)
        return false;

    while (game->running && !WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f)
            dt = 0.05f;

        game_handle_input(game, dt);
        game_update(game, dt);
        game_draw_world(game);
        game_draw(game);
    }

    return true;
}
