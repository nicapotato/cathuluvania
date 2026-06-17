#include "collision.h"
#include "level.h"
#include "tile_config.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool tile_solid_at(const struct Level *level, int col, int row) {
    return tile_is_solid(level_get_tile(level, col, row));
}

static float tile_surface_y(const struct Level *level, int col, int row) {
    int surf = level_get_surface_y(level, col, row);
    if (surf >= 0)
        return (float)surf;
    return (float)(row * TILE_SIZE);
}

static bool tile_has_floor_face(const struct Level *level, int col, int row) {
    return tile_solid_at(level, col, row) && !tile_solid_at(level, col, row - 1);
}

static bool tile_has_ceiling_face(const struct Level *level, int col, int row) {
    return tile_solid_at(level, col, row) && !tile_solid_at(level, col, row + 1);
}

static bool tile_has_left_wall_face(const struct Level *level, int col, int row) {
    return tile_solid_at(level, col, row) && !tile_solid_at(level, col - 1, row);
}

static bool tile_has_right_wall_face(const struct Level *level, int col, int row) {
    return tile_solid_at(level, col, row) && !tile_solid_at(level, col + 1, row);
}

static void wall_span_y(const struct Level *level, int col, int row, float *y_top, float *y_bot) {
    float cell_t = (float)(row * TILE_SIZE);
    float cell_b = (float)((row + 1) * TILE_SIZE);

    *y_top = cell_t;
    if (tile_has_floor_face(level, col, row))
        *y_top = tile_surface_y(level, col, row);

    *y_bot = cell_b;
}

static bool collision_world_push(CollisionWorld *world, CollisionEdge edge) {
    if (world->count >= world->capacity) {
        int new_cap = world->capacity ? world->capacity * 2 : 256;
        CollisionEdge *next = realloc(world->edges, (size_t)new_cap * sizeof(CollisionEdge));
        if (!next)
            return false;
        world->edges = next;
        world->capacity = new_cap;
    }
    world->edges[world->count++] = edge;
    return true;
}

static int float_floor_div(float v, float tile) {
    return (int)floorf(v / tile);
}

static int float_ceil_div(float v, float tile) {
    return (int)ceilf(v / tile);
}

static void collision_grid_free(CollisionSpatialGrid *grid) {
    if (!grid)
        return;
    free(grid->cell_offsets);
    free(grid->cell_indices);
    memset(grid, 0, sizeof(*grid));
}

static bool collision_grid_build(CollisionWorld *world, int cols, int rows) {
    collision_grid_free(&world->grid);

    if (cols <= 0 || rows <= 0 || world->count <= 0)
        return true;

    world->grid.cols = cols;
    world->grid.rows = rows;

    size_t cell_count = (size_t)cols * (size_t)rows;
    world->grid.cell_offsets = calloc(cell_count + 1, sizeof(int));
    if (!world->grid.cell_offsets)
        return false;

    int *counts = calloc(cell_count, sizeof(int));
    if (!counts) {
        collision_grid_free(&world->grid);
        return false;
    }

    for (int i = 0; i < world->count; i++) {
        const CollisionEdge *e = &world->edges[i];
        float min_x = fminf(e->x0, e->x1);
        float max_x = fmaxf(e->x0, e->x1);
        float min_y = fminf(e->y0, e->y1);
        float max_y = fmaxf(e->y0, e->y1);

        int c0 = float_floor_div(min_x, (float)TILE_SIZE);
        int c1 = float_ceil_div(max_x, (float)TILE_SIZE) - 1;
        int r0 = float_floor_div(min_y, (float)TILE_SIZE);
        int r1 = float_ceil_div(max_y, (float)TILE_SIZE) - 1;

        if (c1 < c0)
            c1 = c0;
        if (r1 < r0)
            r1 = r0;

        if (c0 < 0)
            c0 = 0;
        if (r0 < 0)
            r0 = 0;
        if (c1 >= cols)
            c1 = cols - 1;
        if (r1 >= rows)
            r1 = rows - 1;

        for (int r = r0; r <= r1; r++) {
            for (int c = c0; c <= c1; c++)
                counts[r * cols + c]++;
        }
    }

    world->grid.cell_offsets[0] = 0;
    for (size_t i = 0; i < cell_count; i++)
        world->grid.cell_offsets[i + 1] = world->grid.cell_offsets[i] + counts[i];

    int total = world->grid.cell_offsets[cell_count];
    world->grid.cell_indices = malloc((size_t)total * sizeof(int));
    if (!world->grid.cell_indices) {
        free(counts);
        collision_grid_free(&world->grid);
        return false;
    }

    int *cursor = malloc(cell_count * sizeof(int));
    if (!cursor) {
        free(counts);
        collision_grid_free(&world->grid);
        return false;
    }
    for (size_t i = 0; i < cell_count; i++)
        cursor[i] = world->grid.cell_offsets[i];

    for (int i = 0; i < world->count; i++) {
        const CollisionEdge *e = &world->edges[i];
        float min_x = fminf(e->x0, e->x1);
        float max_x = fmaxf(e->x0, e->x1);
        float min_y = fminf(e->y0, e->y1);
        float max_y = fmaxf(e->y0, e->y1);

        int c0 = float_floor_div(min_x, (float)TILE_SIZE);
        int c1 = float_ceil_div(max_x, (float)TILE_SIZE) - 1;
        int r0 = float_floor_div(min_y, (float)TILE_SIZE);
        int r1 = float_ceil_div(max_y, (float)TILE_SIZE) - 1;

        if (c1 < c0)
            c1 = c0;
        if (r1 < r0)
            r1 = r0;

        if (c0 < 0)
            c0 = 0;
        if (r0 < 0)
            r0 = 0;
        if (c1 >= cols)
            c1 = cols - 1;
        if (r1 >= rows)
            r1 = rows - 1;

        for (int r = r0; r <= r1; r++) {
            for (int c = c0; c <= c1; c++) {
                int idx = r * cols + c;
                world->grid.cell_indices[cursor[idx]++] = i;
            }
        }
    }

    free(cursor);
    free(counts);
    return true;
}

static int collision_query_edges(CollisionWorld *world, float min_x, float min_y, float max_x,
                                 float max_y, int *out, int max_out) {
    if (!world || !out || max_out <= 0 || world->grid.cols <= 0 || world->grid.rows <= 0)
        return 0;

    int cols = world->grid.cols;
    int rows = world->grid.rows;

    int c0 = float_floor_div(min_x, (float)TILE_SIZE);
    int c1 = float_ceil_div(max_x, (float)TILE_SIZE) - 1;
    int r0 = float_floor_div(min_y, (float)TILE_SIZE);
    int r1 = float_ceil_div(max_y, (float)TILE_SIZE) - 1;

    if (c1 < c0)
        c1 = c0;
    if (r1 < r0)
        r1 = r0;

    if (c0 < 0)
        c0 = 0;
    if (r0 < 0)
        r0 = 0;
    if (c1 >= cols)
        c1 = cols - 1;
    if (r1 >= rows)
        r1 = rows - 1;

    world->query_generation++;
    if (world->query_generation <= 0)
        world->query_generation = 1;

    if (!world->query_stamp && world->count > 0)
        world->query_stamp = calloc((size_t)world->count, sizeof(int));

    int n = 0;
    for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
            int cell = r * cols + c;
            int start = world->grid.cell_offsets[cell];
            int end = world->grid.cell_offsets[cell + 1];
            for (int i = start; i < end; i++) {
                int edge_i = world->grid.cell_indices[i];
                if (world->query_stamp[edge_i] == world->query_generation)
                    continue;
                world->query_stamp[edge_i] = world->query_generation;
                if (n < max_out)
                    out[n++] = edge_i;
            }
        }
    }
    return n;
}

static void collision_merge_floor_edges(CollisionWorld *world) {
    if (world->count <= 1)
        return;

    bool *removed = calloc((size_t)world->count, sizeof(bool));
    if (!removed)
        return;

    for (int i = 0; i < world->count; i++) {
        if (removed[i] || world->edges[i].kind != COLL_EDGE_FLOOR)
            continue;

        float y = world->edges[i].y0;
        float x0 = world->edges[i].x0;
        float x1 = world->edges[i].x1;

        for (int j = i + 1; j < world->count; j++) {
            if (removed[j] || world->edges[j].kind != COLL_EDGE_FLOOR)
                continue;
            if (fabsf(world->edges[j].y0 - y) > 1.0f)
                continue;

            float ox0 = world->edges[j].x0;
            float ox1 = world->edges[j].x1;

            if (ox0 <= x1 + 1.0f && ox1 >= x0 - 1.0f) {
                if (ox0 < x0)
                    x0 = ox0;
                if (ox1 > x1)
                    x1 = ox1;
                world->edges[i].x0 = x0;
                world->edges[i].x1 = x1;
                removed[j] = true;
            }
        }
    }

    int w = 0;
    for (int i = 0; i < world->count; i++) {
        if (removed[i])
            continue;
        if (w != i)
            world->edges[w] = world->edges[i];
        w++;
    }
    world->count = w;
    free(removed);
}

void collision_world_init(CollisionWorld *world) {
    if (!world)
        return;
    memset(world, 0, sizeof(*world));
}

void collision_world_free(CollisionWorld *world) {
    if (!world)
        return;
    free(world->edges);
    free(world->query_stamp);
    collision_grid_free(&world->grid);
    memset(world, 0, sizeof(*world));
}

bool collision_world_build(CollisionWorld *world, const struct Level *level) {
    if (!world || !level || !level->tiles || level->cols <= 0 || level->rows <= 0)
        return false;

    collision_world_free(world);
    collision_world_init(world);

    for (int r = 0; r < level->rows; r++) {
        for (int c = 0; c < level->cols; c++) {
            if (!tile_solid_at(level, c, r))
                continue;

            float cell_l = (float)(c * TILE_SIZE);
            float cell_r = (float)((c + 1) * TILE_SIZE);
            float span_t, span_b;
            wall_span_y(level, c, r, &span_t, &span_b);

            if (tile_has_floor_face(level, c, r)) {
                float y = tile_surface_y(level, c, r);
                CollisionEdge e = { COLL_EDGE_FLOOR, cell_l, y, cell_r, y };
                if (!collision_world_push(world, e))
                    return false;
            }

            if (tile_has_ceiling_face(level, c, r)) {
                float y = (float)((r + 1) * TILE_SIZE);
                CollisionEdge e = { COLL_EDGE_CEILING, cell_l, y, cell_r, y };
                if (!collision_world_push(world, e))
                    return false;
            }

            if (tile_has_left_wall_face(level, c, r)) {
                CollisionEdge e = { COLL_EDGE_WALL_LEFT, cell_l, span_t, cell_l, span_b };
                if (!collision_world_push(world, e))
                    return false;
            }

            if (tile_has_right_wall_face(level, c, r)) {
                CollisionEdge e = { COLL_EDGE_WALL_RIGHT, cell_r, span_t, cell_r, span_b };
                if (!collision_world_push(world, e))
                    return false;
            }
        }
    }

    collision_merge_floor_edges(world);

    if (!collision_grid_build(world, level->cols, level->rows))
        return false;

    world->query_stamp = calloc((size_t)world->count, sizeof(int));
    return world->query_stamp != NULL;
}

void player_capsule_from_params(const PlayerCapsuleParams *params, float feet_x, float feet_y,
                                PlayerCapsule *out) {
    if (!out)
        return;
    out->valid = false;
    if (!params || !params->valid)
        return;

    out->feet_x = feet_x;
    out->feet_y = feet_y;
    out->half_w = params->half_w;
    out->height = params->height;
    out->corner_r = params->corner_r;
    out->valid = true;
}

void player_capsule_from_pos(const PlayerCapsuleParams *params, float px, float py,
                             PlayerCapsule *out) {
    if (!params || !params->valid) {
        if (out)
            out->valid = false;
        return;
    }
    player_capsule_from_params(params, px, py + PLAYER_HALF, out);
}

float player_capsule_pos_y_from_feet(const PlayerCapsuleParams *params, float feet_y) {
    (void)params;
    return feet_y - PLAYER_HALF;
}

static float capsule_top_y(const PlayerCapsule *c) {
    return c->feet_y - c->height;
}

static float capsule_shoulder_y(const PlayerCapsule *c) {
    return capsule_top_y(c) + c->corner_r;
}

static float capsule_x_left_at_y(const PlayerCapsule *c, float y) {
    float top = capsule_top_y(c);
    float shoulder = capsule_shoulder_y(c);

    if (y >= c->feet_y)
        return c->feet_x - c->half_w;
    if (y < top)
        return c->feet_x - c->half_w;

    if (y >= shoulder)
        return c->feet_x - c->half_w;

    float cx = c->feet_x - c->half_w + c->corner_r;
    float dy = y - shoulder;
    float dx = sqrtf(fmaxf(0.0f, c->corner_r * c->corner_r - dy * dy));
    return cx - dx;
}

static float capsule_x_right_at_y(const PlayerCapsule *c, float y) {
    float top = capsule_top_y(c);
    float shoulder = capsule_shoulder_y(c);

    if (y >= c->feet_y)
        return c->feet_x + c->half_w;
    if (y < top)
        return c->feet_x + c->half_w;

    if (y >= shoulder)
        return c->feet_x + c->half_w;

    float cx = c->feet_x + c->half_w - c->corner_r;
    float dy = y - shoulder;
    float dx = sqrtf(fmaxf(0.0f, c->corner_r * c->corner_r - dy * dy));
    return cx + dx;
}

static float capsule_top_at_x(const PlayerCapsule *c, float x) {
    float top = capsule_top_y(c);
    float shoulder = capsule_shoulder_y(c);
    float dx = fabsf(x - c->feet_x);

    if (dx >= c->half_w)
        return top;
    if (dx <= c->half_w - c->corner_r)
        return shoulder;

    float edge = c->half_w - c->corner_r;
    float nx = (dx - edge) / c->corner_r;
    if (nx > 1.0f)
        nx = 1.0f;
    return shoulder - c->corner_r * sqrtf(fmaxf(0.0f, 1.0f - nx * nx));
}

static bool capsule_overlaps_x(const PlayerCapsule *c, float l, float r) {
    return c->feet_x + c->half_w > l && c->feet_x - c->half_w < r;
}

static void capsule_sample_ys(const PlayerCapsule *c, float *out, int *count) {
    float top = capsule_top_y(c);
    float shoulder = capsule_shoulder_y(c);
    out[0] = c->feet_y - 0.5f;
    out[1] = shoulder + (c->feet_y - shoulder) * 0.5f;
    out[2] = shoulder;
    out[3] = top + c->corner_r * 0.5f;
    out[4] = top + 0.5f;
    *count = COLLISION_WALL_SAMPLE_COUNT;
}

static void sync_pos_from_capsule(const PlayerCapsule *cap, float *px, float *py) {
    *px = cap->feet_x;
    *py = cap->feet_y - PLAYER_HALF;
}

static void apply_ceiling_velocity_response(float *vy, float vx) {
    if (fabsf(vx) < CEILING_STRAIGHT_VX_MAX) {
        if (*vy < CEILING_VY_STOP)
            *vy *= CEILING_VY_DAMP;
        else if (*vy < 0.0f)
            *vy = 0.0f;
    } else if (*vy < 0.0f) {
        *vy = 0.0f;
    }
}

static void apply_ceiling_soft_pad(CollisionWorld *world, PlayerCapsule *cap, float *vy,
                                   const int *candidates, int candidate_count) {
    if (!vy || *vy >= 0.0f || !cap->valid)
        return;

    float best_gap = CEILING_SOFT_PAD + 1.0f;

    for (int ci = 0; ci < candidate_count; ci++) {
        const CollisionEdge *e = &world->edges[candidates[ci]];
        if (e->kind != COLL_EDGE_CEILING)
            continue;
        if (!capsule_overlaps_x(cap, e->x0, e->x1))
            continue;

        float sample_x = fmaxf(e->x0, fminf(cap->feet_x, e->x1));
        if (cap->feet_y <= e->y0)
            continue;

        float top_y = capsule_top_at_x(cap, sample_x);
        float gap = e->y0 - top_y;
        if (gap > 0.0f && gap < best_gap)
            best_gap = gap;
    }

    if (best_gap <= 0.0f || best_gap > CEILING_SOFT_PAD)
        return;

    float t = 1.0f - best_gap / CEILING_SOFT_PAD;
    *vy *= (1.0f - t * 0.92f);
}

static bool player_can_land_on_floor(const PlayerCapsule *c, float l, float r, float floor_y) {
    if (!capsule_overlaps_x(c, l, r))
        return false;
    float sample_x = fmaxf(l, fminf(c->feet_x, r));
    return capsule_top_at_x(c, sample_x) <= floor_y;
}

static bool player_approaches_ceiling_from_below(const PlayerCapsule *c, float l, float r,
                                                 float ceiling_y) {
    if (!capsule_overlaps_x(c, l, r))
        return false;
    if (c->feet_y <= ceiling_y)
        return false;
    float sample_x = fmaxf(l, fminf(c->feet_x, r));
    return capsule_top_at_x(c, sample_x) < ceiling_y;
}

static float capsule_ceiling_penetration(const PlayerCapsule *c, float l, float r, float ceiling_y) {
    if (!player_approaches_ceiling_from_below(c, l, r, ceiling_y))
        return 0.0f;
    float sample_x = fmaxf(l, fminf(c->feet_x, r));
    return ceiling_y - capsule_top_at_x(c, sample_x);
}

static float capsule_floor_penetration(const PlayerCapsule *c, float l, float r, float floor_y) {
    if (!capsule_overlaps_x(c, l, r))
        return 0.0f;
    if (c->feet_y <= floor_y)
        return 0.0f;
    if (!player_can_land_on_floor(c, l, r, floor_y))
        return 0.0f;
    float pen = c->feet_y - floor_y;
    if (pen > COLLISION_MAX_PENETRATION)
        pen = COLLISION_MAX_PENETRATION;
    if (pen > MAX_STEP_HEIGHT + GROUNDED_EPSILON && MAX_STEP_HEIGHT > 0.0f)
        return 0.0f;
    return pen;
}

static float capsule_wall_penetration_left(const PlayerCapsule *c, const CollisionEdge *e) {
    float ys[COLLISION_WALL_SAMPLE_COUNT];
    int count = 0;
    capsule_sample_ys(c, ys, &count);

    float best = 0.0f;
    for (int i = 0; i < count; i++) {
        float y = ys[i];
        if (y < e->y0 || y > e->y1)
            continue;
        float left_x = capsule_x_left_at_y(c, y);
        float right_x = capsule_x_right_at_y(c, y);
        if (right_x <= e->x0 || left_x >= e->x0)
            continue;
        float pen = e->x0 - left_x;
        if (pen > best)
            best = pen;
    }
    return best;
}

static float capsule_wall_penetration_right(const PlayerCapsule *c, const CollisionEdge *e) {
    float ys[COLLISION_WALL_SAMPLE_COUNT];
    int count = 0;
    capsule_sample_ys(c, ys, &count);

    float best = 0.0f;
    for (int i = 0; i < count; i++) {
        float y = ys[i];
        if (y < e->y0 || y > e->y1)
            continue;
        float left_x = capsule_x_left_at_y(c, y);
        float right_x = capsule_x_right_at_y(c, y);
        if (left_x >= e->x0 || right_x <= e->x0)
            continue;
        float pen = right_x - e->x0;
        if (pen > best)
            best = pen;
    }
    return best;
}

static bool capsule_wall_overlap(const PlayerCapsule *c, const CollisionEdge *e) {
    return capsule_wall_penetration_left(c, e) > COLLISION_SKIN ||
           capsule_wall_penetration_right(c, e) > COLLISION_SKIN;
}

static bool capsule_ceiling_overlap(const PlayerCapsule *c, const CollisionEdge *e) {
    return capsule_ceiling_penetration(c, e->x0, e->x1, e->y0) > COLLISION_SKIN;
}

static bool capsule_floor_sweep_hit(const PlayerCapsule *c, float l, float r, float floor_y) {
    if (!capsule_overlaps_x(c, l, r))
        return false;
    if (c->feet_y < floor_y - COLLISION_SKIN)
        return false;

    float sample_x = fmaxf(l, fminf(c->feet_x, r));
    float top = capsule_top_at_x(c, sample_x);
    if (top > floor_y + 2.0f)
        return false;

    return true;
}

static bool capsule_floor_overlap(const PlayerCapsule *c, const CollisionEdge *e) {
    return capsule_floor_sweep_hit(c, e->x0, e->x1, e->y0);
}

static PlayerCapsule capsule_at_offset(const PlayerCapsule *base, float dx, float dy) {
    PlayerCapsule c = *base;
    c.feet_x += dx;
    c.feet_y += dy;
    return c;
}

static float sweep_walls_toi(CollisionWorld *world, const PlayerCapsule *start, float dx,
                             const int *candidates, int candidate_count) {
    if (fabsf(dx) < 1e-6f)
        return 1.0f;

    PlayerCapsule end = capsule_at_offset(start, dx, 0.0f);
    bool hit_end = false;

    for (int ci = 0; ci < candidate_count; ci++) {
        const CollisionEdge *e = &world->edges[candidates[ci]];
        if (e->kind != COLL_EDGE_WALL_LEFT && e->kind != COLL_EDGE_WALL_RIGHT)
            continue;
        if (capsule_wall_overlap(&end, e)) {
            hit_end = true;
            break;
        }
    }
    if (!hit_end)
        return 1.0f;

    float lo = 0.0f;
    float hi = 1.0f;
    for (int i = 0; i < COLLISION_SWEEP_ITER; i++) {
        float mid = (lo + hi) * 0.5f;
        PlayerCapsule mid_cap = capsule_at_offset(start, dx * mid, 0.0f);
        bool hit = false;
        for (int ci = 0; ci < candidate_count; ci++) {
            const CollisionEdge *e = &world->edges[candidates[ci]];
            if (e->kind != COLL_EDGE_WALL_LEFT && e->kind != COLL_EDGE_WALL_RIGHT)
                continue;
            if (capsule_wall_overlap(&mid_cap, e)) {
                hit = true;
                break;
            }
        }
        if (hit)
            hi = mid;
        else
            lo = mid;
    }

    float t = lo;
    if (t < 0.0f)
        t = 0.0f;
    return t;
}

static float sweep_vertical_toi(CollisionWorld *world, const PlayerCapsule *start, float dy,
                                bool down, const int *candidates, int candidate_count) {
    if (fabsf(dy) < 1e-6f)
        return 1.0f;

    PlayerCapsule end = capsule_at_offset(start, 0.0f, dy);
    bool hit_end = false;

    for (int ci = 0; ci < candidate_count; ci++) {
        const CollisionEdge *e = &world->edges[candidates[ci]];
        if (down) {
            if (e->kind == COLL_EDGE_FLOOR && capsule_floor_overlap(&end, e))
                hit_end = true;
        } else {
            if (e->kind == COLL_EDGE_CEILING && capsule_ceiling_overlap(&end, e))
                hit_end = true;
        }
        if (hit_end)
            break;
    }
    if (!hit_end)
        return 1.0f;

    float lo = 0.0f;
    float hi = 1.0f;
    for (int i = 0; i < COLLISION_SWEEP_ITER; i++) {
        float mid = (lo + hi) * 0.5f;
        PlayerCapsule mid_cap = capsule_at_offset(start, 0.0f, dy * mid);
        bool hit = false;
        for (int ci = 0; ci < candidate_count; ci++) {
            const CollisionEdge *e = &world->edges[candidates[ci]];
            if (down) {
                if (e->kind == COLL_EDGE_FLOOR && capsule_floor_overlap(&mid_cap, e))
                    hit = true;
            } else {
                if (e->kind == COLL_EDGE_CEILING && capsule_ceiling_overlap(&mid_cap, e))
                    hit = true;
            }
            if (hit)
                break;
        }
        if (hit)
            hi = mid;
        else
            lo = mid;
    }

    return lo;
}

static void resolve_wall_at_pose(CollisionWorld *world, PlayerCapsule *cap, float *px, float *py,
                                 float *vx, bool *wall_left, bool *wall_right,
                                 const int *candidates, int candidate_count) {
    for (int iter = 0; iter < COLLISION_DEPENETRATE_ITER; iter++) {
        float best_pen = 0.0f;
        int best_dir = 0;

        for (int ci = 0; ci < candidate_count; ci++) {
            const CollisionEdge *e = &world->edges[candidates[ci]];
            if (e->kind == COLL_EDGE_WALL_LEFT) {
                float pen = capsule_wall_penetration_left(cap, e);
                if (pen > best_pen) {
                    best_pen = pen;
                    best_dir = -1;
                }
            } else if (e->kind == COLL_EDGE_WALL_RIGHT) {
                float pen = capsule_wall_penetration_right(cap, e);
                if (pen > best_pen) {
                    best_pen = pen;
                    best_dir = 1;
                }
            }
        }

        if (best_pen <= COLLISION_SKIN)
            break;

        cap->feet_x -= (float)best_dir * best_pen;
        sync_pos_from_capsule(cap, px, py);

        if (best_dir < 0) {
            *wall_left = true;
            if (*vx > 0.0f)
                *vx = 0.0f;
        } else {
            *wall_right = true;
            if (*vx < 0.0f)
                *vx = 0.0f;
        }
    }
}

static void resolve_floor_at_pose(CollisionWorld *world, PlayerCapsule *cap, float *px, float *py,
                                  float *vy, bool *grounded, const int *candidates,
                                  int candidate_count) {
    float best_floor_y = 0.0f;
    bool found = false;

    for (int ci = 0; ci < candidate_count; ci++) {
        const CollisionEdge *e = &world->edges[candidates[ci]];
        if (e->kind != COLL_EDGE_FLOOR)
            continue;
        if (!capsule_floor_sweep_hit(cap, e->x0, e->x1, e->y0))
            continue;
        if (!found || e->y0 > best_floor_y) {
            best_floor_y = e->y0;
            found = true;
        }
    }

    if (found) {
        cap->feet_y = best_floor_y;
        sync_pos_from_capsule(cap, px, py);
        *vy = 0.0f;
        *grounded = true;
    }
}

static void resolve_ceiling_at_pose(CollisionWorld *world, PlayerCapsule *cap, float *px, float *py,
                                    float *vx, float *vy, const int *candidates,
                                    int candidate_count) {
    float best_pen = 0.0f;

    for (int ci = 0; ci < candidate_count; ci++) {
        const CollisionEdge *e = &world->edges[candidates[ci]];
        if (e->kind != COLL_EDGE_CEILING)
            continue;
        float pen = capsule_ceiling_penetration(cap, e->x0, e->x1, e->y0);
        if (pen > best_pen)
            best_pen = pen;
    }

    if (best_pen > COLLISION_SKIN) {
        cap->feet_y += best_pen;
        sync_pos_from_capsule(cap, px, py);
        apply_ceiling_velocity_response(vy, *vx);
        (void)world;
    }
}

static void depenetrate_safety(CollisionWorld *world, PlayerCapsule *cap, float *px, float *py,
                               const int *candidates, int candidate_count) {
    for (int iter = 0; iter < COLLISION_DEPENETRATE_ITER; iter++) {
        float best_pen = 0.0f;
        int axis = 0;
        int dir = 0;

        for (int ci = 0; ci < candidate_count; ci++) {
            const CollisionEdge *e = &world->edges[candidates[ci]];
            float pen = 0.0f;

            if (e->kind == COLL_EDGE_WALL_LEFT) {
                pen = capsule_wall_penetration_left(cap, e);
                if (pen > best_pen) {
                    best_pen = pen;
                    axis = 0;
                    dir = -1;
                }
            } else if (e->kind == COLL_EDGE_WALL_RIGHT) {
                pen = capsule_wall_penetration_right(cap, e);
                if (pen > best_pen) {
                    best_pen = pen;
                    axis = 0;
                    dir = 1;
                }
            } else if (e->kind == COLL_EDGE_FLOOR) {
                pen = capsule_floor_penetration(cap, e->x0, e->x1, e->y0);
                if (pen > best_pen) {
                    best_pen = pen;
                    axis = 1;
                    dir = -1;
                }
            } else if (e->kind == COLL_EDGE_CEILING) {
                pen = capsule_ceiling_penetration(cap, e->x0, e->x1, e->y0);
                if (pen > best_pen) {
                    best_pen = pen;
                    axis = 1;
                    dir = 1;
                }
            }
        }

        if (best_pen <= COLLISION_SKIN)
            break;

        if (axis == 0)
            cap->feet_x -= (float)dir * best_pen;
        else
            cap->feet_y += (float)dir * best_pen;
        sync_pos_from_capsule(cap, px, py);
        (void)world;
    }
}

static void capsule_query_bounds(const PlayerCapsule *cap, float pad_x, float pad_y, float *min_x,
                               float *min_y, float *max_x, float *max_y) {
    *min_x = cap->feet_x - cap->half_w - pad_x;
    *max_x = cap->feet_x + cap->half_w + pad_x;
    *min_y = capsule_top_y(cap) - pad_y;
    *max_y = cap->feet_y + pad_y;
}

static void sweep_move_substep(CollisionWorld *world, PlayerCapsule *cap, float *px, float *py,
                             float *vx, float *vy, float dt, bool *wall_left, bool *wall_right,
                             bool *grounded) {
    float dx = *vx * dt;
    float dy = *vy * dt;

    float pad_x = fabsf(dx) + cap->half_w + 8.0f;
    float pad_y = fabsf(dy) + cap->height + 8.0f;

    int candidates[COLLISION_GRID_QUERY_MAX];
    float qmin_x, qmin_y, qmax_x, qmax_y;
    capsule_query_bounds(cap, pad_x, pad_y, &qmin_x, &qmin_y, &qmax_x, &qmax_y);
    int candidate_count =
        collision_query_edges(world, qmin_x, qmin_y, qmax_x, qmax_y, candidates,
                              COLLISION_GRID_QUERY_MAX);

    if (fabsf(dx) > 1e-6f) {
        float t_x = sweep_walls_toi(world, cap, dx, candidates, candidate_count);
        if (t_x < 1.0f) {
            dx *= t_x;
            dx -= copysignf(COLLISION_SKIN, dx);
            if (fabsf(dx) < COLLISION_SKIN)
                dx = 0.0f;
            if (*vx > 0.0f)
                *wall_right = true;
            else if (*vx < 0.0f)
                *wall_left = true;
            if ((*vx > 0.0f && t_x < 1.0f) || (*vx < 0.0f && t_x < 1.0f))
                *vx = 0.0f;
        }
        cap->feet_x += dx;
        sync_pos_from_capsule(cap, px, py);
        resolve_wall_at_pose(world, cap, px, py, vx, wall_left, wall_right, candidates,
                             candidate_count);
    }

    if (fabsf(dy) > 1e-6f) {
        bool down = dy > 0.0f;
        float t_y = sweep_vertical_toi(world, cap, dy, down, candidates, candidate_count);
        if (t_y < 1.0f) {
            dy *= t_y;
            dy -= copysignf(COLLISION_SKIN, dy);
            if (fabsf(dy) < COLLISION_SKIN)
                dy = 0.0f;
            if (down) {
                *vy = 0.0f;
                *grounded = true;
            } else {
                apply_ceiling_velocity_response(vy, *vx);
            }
        }
        cap->feet_y += dy;
        sync_pos_from_capsule(cap, px, py);

        if (down)
            resolve_floor_at_pose(world, cap, px, py, vy, grounded, candidates, candidate_count);
        else
            resolve_ceiling_at_pose(world, cap, px, py, vx, vy, candidates, candidate_count);
    }
}

static void floor_snap(CollisionWorld *world, PlayerCapsule *cap, float *px, float *py, float *vy,
                       bool *grounded) {
    if (*grounded || *vy < 0.0f || !cap->valid)
        return;

    float best_gap = COLLISION_FLOOR_SNAP_DIST + 1.0f;
    float best_y = 0.0f;
    bool found = false;

    int candidates[COLLISION_GRID_QUERY_MAX];
    float qmin_x = cap->feet_x - cap->half_w - 4.0f;
    float qmax_x = cap->feet_x + cap->half_w + 4.0f;
    float qmin_y = cap->feet_y;
    float qmax_y = cap->feet_y + COLLISION_FLOOR_SNAP_DIST + 4.0f;
    int candidate_count =
        collision_query_edges(world, qmin_x, qmin_y, qmax_x, qmax_y, candidates,
                              COLLISION_GRID_QUERY_MAX);

    for (int ci = 0; ci < candidate_count; ci++) {
        const CollisionEdge *e = &world->edges[candidates[ci]];
        if (e->kind != COLL_EDGE_FLOOR)
            continue;
        if (!player_can_land_on_floor(cap, e->x0, e->x1, e->y0))
            continue;

        float gap = e->y0 - cap->feet_y;
        if (gap < -COLLISION_SKIN || gap > COLLISION_FLOOR_SNAP_DIST)
            continue;
        if (gap < best_gap) {
            best_gap = gap;
            best_y = e->y0;
            found = true;
        }
    }

    if (!found)
        return;

    cap->feet_y = best_y;
    sync_pos_from_capsule(cap, px, py);
    *vy = 0.0f;
    *grounded = true;
}

static int compute_substeps(float vx, float vy, float dt) {
    float max_disp = fmaxf(fabsf(vx), fabsf(vy)) * dt;
    int substeps = (int)ceilf(max_disp / COLLISION_MAX_STEP_PX);
    if (substeps < 1)
        substeps = 1;
    if (substeps > COLLISION_MAX_SUBSTEPS)
        substeps = COLLISION_MAX_SUBSTEPS;
    return substeps;
}

CollisionMoveResult collision_move_player(CollisionWorld *world, PlayerCapsule *cap, float *px,
                                          float *py, float *vx, float *vy, float dt) {
    CollisionMoveResult result = { false, false, false };
    if (!world || !cap || !cap->valid || !px || !py || !vx || !vy || dt <= 0.0f)
        return result;

    int candidates[COLLISION_GRID_QUERY_MAX];
    float qmin_x, qmin_y, qmax_x, qmax_y;
    capsule_query_bounds(cap, cap->half_w + 16.0f, cap->height + 16.0f, &qmin_x, &qmin_y, &qmax_x,
                         &qmax_y);
    int candidate_count =
        collision_query_edges(world, qmin_x, qmin_y, qmax_x, qmax_y, candidates,
                              COLLISION_GRID_QUERY_MAX);
    apply_ceiling_soft_pad(world, cap, vy, candidates, candidate_count);

    int substeps = compute_substeps(*vx, *vy, dt);
    float sub_dt = dt / (float)substeps;
    float vx_step = *vx;
    float vy_step = *vy;

    for (int s = 0; s < substeps; s++) {
        sweep_move_substep(world, cap, px, py, &vx_step, &vy_step, sub_dt, &result.wall_left,
                           &result.wall_right, &result.grounded);
    }

    *vx = vx_step;
    *vy = vy_step;

    floor_snap(world, cap, px, py, vy, &result.grounded);

    if (!result.grounded)
        result.grounded = collision_check_grounded(world, cap);

    return result;
}

void collision_resolve_overlap(CollisionWorld *world, PlayerCapsule *cap, float *px, float *py) {
    if (!world || !cap || !cap->valid || !px || !py)
        return;

    int candidates[COLLISION_GRID_QUERY_MAX];
    float qmin_x, qmin_y, qmax_x, qmax_y;
    capsule_query_bounds(cap, cap->half_w + 32.0f, cap->height + 32.0f, &qmin_x, &qmin_y, &qmax_x,
                         &qmax_y);
    int candidate_count =
        collision_query_edges(world, qmin_x, qmin_y, qmax_x, qmax_y, candidates,
                              COLLISION_GRID_QUERY_MAX);
    depenetrate_safety(world, cap, px, py, candidates, candidate_count);
}

bool collision_check_grounded(const CollisionWorld *world, const PlayerCapsule *cap) {
    if (!world || !cap || !cap->valid)
        return false;

    CollisionWorld *mutable_world = (CollisionWorld *)world;
    int candidates[COLLISION_GRID_QUERY_MAX];
    float qmin_x = cap->feet_x - cap->half_w - 2.0f;
    float qmax_x = cap->feet_x + cap->half_w + 2.0f;
    float qmin_y = cap->feet_y - GROUNDED_EPSILON - 2.0f;
    float qmax_y = cap->feet_y + 2.0f;
    int candidate_count =
        collision_query_edges(mutable_world, qmin_x, qmin_y, qmax_x, qmax_y, candidates,
                              COLLISION_GRID_QUERY_MAX);

    for (int ci = 0; ci < candidate_count; ci++) {
        const CollisionEdge *e = &world->edges[candidates[ci]];
        if (e->kind != COLL_EDGE_FLOOR)
            continue;
        if (!player_can_land_on_floor(cap, e->x0, e->x1, e->y0))
            continue;
        float dist = cap->feet_y - e->y0;
        if (dist >= 0.0f && dist <= GROUNDED_EPSILON)
            return true;
    }
    return false;
}

bool player_capsule_overlaps_aabb(const PlayerCapsule *cap, float l, float t, float r, float b) {
    if (!cap || !cap->valid)
        return false;
    if (cap->feet_x + cap->half_w <= l || cap->feet_x - cap->half_w >= r)
        return false;
    if (cap->feet_y <= t || capsule_top_y(cap) >= b)
        return false;
    return true;
}

void collision_draw_world_debug(const CollisionWorld *world, float cam_x, float cam_y,
                                float view_w, float view_h) {
    if (!world)
        return;

    float min_x = cam_x - 32.0f;
    float max_x = cam_x + view_w + 32.0f;
    float min_y = cam_y - 32.0f;
    float max_y = cam_y + view_h + 32.0f;

    for (int i = 0; i < world->count; i++) {
        const CollisionEdge *e = &world->edges[i];
        float ex0 = e->x0;
        float ey0 = e->y0;
        float ex1 = e->x1;
        float ey1 = e->y1;

        if (ex0 < min_x && ex1 < min_x)
            continue;
        if (ex0 > max_x && ex1 > max_x)
            continue;
        if (ey0 < min_y && ey1 < min_y)
            continue;
        if (ey0 > max_y && ey1 > max_y)
            continue;

        Color col = (Color){ 50, 255, 100, 220 };
        DrawLineV((Vector2){ ex0, ey0 }, (Vector2){ ex1, ey1 }, col);
    }
}

void collision_draw_capsule_debug(const PlayerCapsule *cap) {
    if (!cap || !cap->valid)
        return;

    float shoulder = capsule_shoulder_y(cap);
    float l = cap->feet_x - cap->half_w;
    float r = cap->feet_x + cap->half_w;

    DrawLineV((Vector2){ l, cap->feet_y }, (Vector2){ r, cap->feet_y }, (Color){ 80, 220, 255, 255 });
    DrawLineV((Vector2){ l, shoulder }, (Vector2){ l, cap->feet_y }, (Color){ 80, 220, 255, 255 });
    DrawLineV((Vector2){ r, shoulder }, (Vector2){ r, cap->feet_y }, (Color){ 80, 220, 255, 255 });

    int arc_steps = 8;
    float prev_x = cap->feet_x - cap->half_w;
    float prev_y = shoulder;
    for (int i = 1; i <= arc_steps; i++) {
        float t = (float)i / (float)arc_steps;
        float y = shoulder - cap->corner_r * t;
        float x = capsule_x_left_at_y(cap, y);
        DrawLineV((Vector2){ prev_x, prev_y }, (Vector2){ x, y }, (Color){ 80, 220, 255, 255 });
        prev_x = x;
        prev_y = y;
    }

    prev_x = cap->feet_x + cap->half_w;
    prev_y = shoulder;
    for (int i = 1; i <= arc_steps; i++) {
        float t = (float)i / (float)arc_steps;
        float y = shoulder - cap->corner_r * t;
        float x = capsule_x_right_at_y(cap, y);
        DrawLineV((Vector2){ prev_x, prev_y }, (Vector2){ x, y }, (Color){ 80, 220, 255, 255 });
        prev_x = x;
        prev_y = y;
    }

    DrawLine((int)(cap->feet_x - 4.0f), (int)cap->feet_y, (int)(cap->feet_x + 4.0f), (int)cap->feet_y,
             GREEN);
}
