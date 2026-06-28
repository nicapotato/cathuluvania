#ifndef MAIN_H
#define MAIN_H

#include "acts.h"
#include "raylib.h"

#define WINDOW_TITLE "Cathuluvania"

#define TILE_SIZE 16

#define VIEW_WIDTH 320
#define VIEW_HEIGHT 240

#define MIN_WINDOW_SCALE 3
#define DEFAULT_WINDOW_SCALE 3

#define MIN_WINDOW_WIDTH (VIEW_WIDTH * MIN_WINDOW_SCALE)
#define MIN_WINDOW_HEIGHT (VIEW_HEIGHT * MIN_WINDOW_SCALE)
#define DEFAULT_WINDOW_WIDTH MIN_WINDOW_WIDTH
#define DEFAULT_WINDOW_HEIGHT MIN_WINDOW_HEIGHT

#define PARALLAX_FACTOR 0.35f

#define GRAVITY 1000.0f
#define MOVE_SPEED 480.0f
#define AIR_MOVE_SPEED 320.0f
#define MAX_RUN_SPEED 120.0f
#define JUMP_VELOCITY_MAX -396.0f  /* sqrt(2) * 280 — doubles max jump height vs -280 */
#define JUMP_CUT_VY_MULTIPLIER 0.45f  /* applied once on jump release while rising */

#define WALL_SLIDE_MAX_VY 45.0f
#define GLIDE_GRAVITY 350.0f
#define GLIDE_MAX_VY 55.0f

#define DASH_WINDUP_TIME 0.4f
#define DASH_CHARGE_MAX_TIME 0.8f
#define DASH_SPEED_WEAK 120.0f
#define DASH_SPEED_MIN 280.0f
#define DASH_SPEED_MAX 520.0f
#define DASH_SLOWMO_SCALE 0.25f
#define DASH_GRAVITY 200.0f

#define PLAYER_JUMP_CAPACITY_DEFAULT  1
#define PLAYER_DASH_CAPACITY_DEFAULT  1
#define PLAYER_GLIDE_CAPACITY_DEFAULT 1
#define PLAYER_UPGRADE_CAPACITY_MAX   9

#define COYOTE_TIME 0.12f
#define JUMP_BUFFER_TIME 0.12f
#define GROUNDED_EPSILON 2.0f

#define SPAWN_DROP_HEIGHT 24.0f

#define PLAYER_SIZE 12.0f
#define PLAYER_HALF (PLAYER_SIZE * 0.5f)

/* Narrower/feet-biased body for horizontal walls (capsule-ish AABB). */
#define PLAYER_HALF_X 4.0f
#define PLAYER_HALF_Y 5.0f
#define PLAYER_COLLIDE_Y_BIAS 2.0f

#define PLAYER_SPRITE_RUN_THRESHOLD 8.0f
#define PLAYER_JUMP_IN_TIME 0.12f

/* 1.0 = one Aseprite canvas pixel maps to one world pixel (crisp pixel art).
 * Use only 1.0 or whole-number scales; fractional values (e.g. 0.6) downsample. */
#define PLAYER_SPRITE_DRAW_SCALE 1.0f

/* Shrink art collision bbox horizontally (world px per side) for tighter wall slides. */
#define PLAYER_COLLISION_SHAVE_X 2.0f

/* Tall oval hitbox derived from art collision bbox (rx narrow, ry full height). */
#define PLAYER_COLLISION_OVAL_RX_SCALE 0.45f
#define PLAYER_COLLISION_OVAL_RY_SCALE 0.97f

/* Gameplay capsule (Hollow Knight style flat feet + rounded top). */
#define PLAYER_CAPSULE_HEIGHT ((float)(TILE_SIZE * 2))
#define PLAYER_CAPSULE_CORNER_R 5.0f
#define MAX_STEP_HEIGHT 0.0f
#define COLLISION_MAX_STEP_PX 4.0f
#define COLLISION_MAX_SUBSTEPS 8
#define COLLISION_SKIN 0.5f
#define COLLISION_FLOOR_SNAP_DIST 12.0f
#define COLLISION_MAX_PENETRATION 16.0f
#define COLLISION_DEPENETRATE_ITER 10
#define COLLISION_WALL_SAMPLE_COUNT 5
#define COLLISION_SWEEP_ITER 8
#define COLLISION_GRID_QUERY_MAX 512

/* Ceiling soft landing: pad below surfaces + velocity response. */
#define CEILING_SOFT_PAD 14.0f
#define CEILING_STRAIGHT_VX_MAX 40.0f
#define CEILING_VY_DAMP 0.15f
#define CEILING_VY_STOP -60.0f

#define CAMERA_FOLLOW_SPEED 12.0f

#endif
