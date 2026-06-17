#include "acts.h"
#include "collision.h"
#include "level.h"
#include "main.h"
#include "platform_path.h"
#include "player_sprite.h"
#include "raylib.h"
#include "tile_config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_FRAMES 60
#define TEST_MAX_TELEPORT 32.0f

static int g_failures = 0;

static void expect_true(const char *name, bool cond) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        g_failures++;
    } else {
        printf("ok: %s\n", name);
    }
}

static void expect_near(const char *name, float a, float b, float eps) {
    if (fabsf(a - b) > eps) {
        fprintf(stderr, "FAIL: %s (got %.2f expected %.2f)\n", name, a, b);
        g_failures++;
    } else {
        printf("ok: %s\n", name);
    }
}

static bool load_dark_act(Level *level, PlayerSprite *sprite, PlayerCapsuleParams *params) {
    if (!level_load(level, &ACTS[1]))
        return false;
    if (!player_sprite_load(sprite, PLAYER_SPRITE_ASSET_PATH))
        return false;
    *params = sprite->capsule_params;
    return params->valid;
}

static void simulate_frames(Level *level, PlayerCapsuleParams *params, float *px, float *py,
                            float *vx, float *vy, int frames, float dt) {
    PlayerCapsule cap;
    for (int i = 0; i < frames; i++) {
        *vy += GRAVITY * dt;
        player_capsule_from_pos(params, *px, *py, &cap);
        collision_move_player(&level->collision, &cap, px, py, vx, vy, dt);
    }
}

static bool capsule_overlaps_solid(const Level *level, const PlayerCapsule *cap) {
    int c0 = (int)floorf((cap->feet_x - cap->half_w) / TILE_SIZE);
    int c1 = (int)floorf((cap->feet_x + cap->half_w) / TILE_SIZE);
    int r0 = (int)floorf((cap->feet_y - cap->height) / TILE_SIZE);
    int r1 = (int)floorf(cap->feet_y / TILE_SIZE);

    for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
            if (tile_is_solid(level_get_tile(level, c, r)))
                return true;
        }
    }
    return false;
}

static void test_spawn_stable(Level *level, PlayerCapsuleParams *params) {
    level_set_active_save(level, 0);
    float px = level->spawn.x;
    float py = level->spawn.y;
    float vx = 0.0f;
    float vy = 0.0f;
    float sx = px;

    simulate_frames(level, params, &px, &py, &vx, &vy, TEST_FRAMES, 1.0f / 60.0f);

    PlayerCapsule cap;
    player_capsule_from_pos(params, px, py, &cap);
    expect_near("spawn save-0 x stable", px, sx, 1.0f);
    expect_true("spawn save-0 lands grounded", collision_check_grounded(&level->collision, &cap));
    expect_true("spawn save-0 stays in bounds", cap.feet_y < (float)level->height);
}

static void test_fall_lands(Level *level, PlayerCapsuleParams *params) {
    level_set_active_save(level, 0);
    float px = level->spawn.x;
    float py = level->spawn.y - 120.0f;
    float vx = 0.0f;
    float vy = 0.0f;

    simulate_frames(level, params, &px, &py, &vx, &vy, 120, 1.0f / 60.0f);

    PlayerCapsule cap;
    player_capsule_from_pos(params, px, py, &cap);
    expect_true("fall lands grounded", collision_check_grounded(&level->collision, &cap));
    expect_true("fall feet above world bottom", cap.feet_y < (float)level->height);
}

static void test_wall_no_teleport(Level *level, PlayerCapsuleParams *params) {
    level_set_active_save(level, 0);
    float px = 200.0f;
    float py = 200.0f;
    float vx = 400.0f;
    float vy = -300.0f;

    for (int i = 0; i < 30; i++) {
        float prev_x = px;
        PlayerCapsule cap;
        vy += GRAVITY * (1.0f / 60.0f);
        player_capsule_from_pos(params, px, py, &cap);
        collision_move_player(&level->collision, &cap, &px, &py, &vx, &vy, 1.0f / 60.0f);
        if (fabsf(px - prev_x) > TEST_MAX_TELEPORT) {
            expect_true("wall jump no large x teleport", false);
            return;
        }
    }
    expect_true("wall jump no large x teleport", true);
}

static void test_no_solid_overlap_after_move(Level *level, PlayerCapsuleParams *params) {
    level_set_active_save(level, 0);
    float px = level->spawn.x;
    float py = level->spawn.y;
    float vx = 280.0f;
    float vy = -396.0f;

    for (int i = 0; i < 45; i++) {
        PlayerCapsule cap;
        vy += GRAVITY * (1.0f / 60.0f);
        player_capsule_from_pos(params, px, py, &cap);
        collision_move_player(&level->collision, &cap, &px, &py, &vx, &vy, 1.0f / 60.0f);
        player_capsule_from_pos(params, px, py, &cap);
        if (capsule_overlaps_solid(level, &cap)) {
            expect_true("diagonal move no solid overlap", false);
            return;
        }
    }
    expect_true("diagonal move no solid overlap", true);
}

int main(void) {
    app_set_resource_root();
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 240, "collision-test");

    Level level = { 0 };
    PlayerSprite sprite = { 0 };
    PlayerCapsuleParams params = { 0 };

    if (!load_dark_act(&level, &sprite, &params)) {
        fprintf(stderr, "collision-test: setup failed\n");
        CloseWindow();
        return 1;
    }

    printf("collision-test: dark-act edges=%d\n", level.collision.count);
    test_spawn_stable(&level, &params);
    test_fall_lands(&level, &params);
    test_wall_no_teleport(&level, &params);
    test_no_solid_overlap_after_move(&level, &params);

    player_sprite_unload(&sprite);
    level_free(&level);
    CloseWindow();

    if (g_failures > 0) {
        fprintf(stderr, "collision-test: %d failure(s)\n", g_failures);
        return 1;
    }

    printf("collision-test: all passed\n");
    return 0;
}
