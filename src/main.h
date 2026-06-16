#ifndef MAIN_H
#define MAIN_H

#include "acts.h"
#include "raylib.h"

#define WINDOW_TITLE "Cathuluvania"

#define TILE_SIZE 16

#define VIEW_WIDTH 320
#define VIEW_HEIGHT 240

#define WINDOW_SCALE 3
#define WINDOW_WIDTH (VIEW_WIDTH * WINDOW_SCALE)
#define WINDOW_HEIGHT (VIEW_HEIGHT * WINDOW_SCALE)

#define PARALLAX_FACTOR 0.35f

#define GRAVITY 1000.0f
#define MOVE_SPEED 480.0f
#define AIR_MOVE_SPEED 320.0f
#define MAX_RUN_SPEED 120.0f
#define JUMP_VELOCITY -280.0f
#define WALL_SLIDE_MAX_VY 45.0f

#define COYOTE_TIME 0.12f
#define JUMP_BUFFER_TIME 0.12f
#define GROUNDED_EPSILON 2.0f
#define COLLISION_SKIN 1.0f

#define SPAWN_DROP_HEIGHT 24.0f

#define PLAYER_SIZE 12.0f
#define PLAYER_HALF (PLAYER_SIZE * 0.5f)

/* Narrower/feet-biased body for horizontal walls (capsule-ish AABB). */
#define PLAYER_HALF_X 4.0f
#define PLAYER_HALF_Y 5.0f
#define PLAYER_COLLIDE_Y_BIAS 2.0f

#endif
