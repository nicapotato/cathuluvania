#include "level.h"
#include "act_export.h"
#include "gameplay_io.h"
#include "tile_catalog.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

void level_sync_collision_from_gameplay(Level *level, const TileCatalog *catalog) {
    level_sync_from_collision_image(level, catalog);
}

static bool cell_opaque_bounds(const Image *image, int col, int row, int *out_min_y, int *out_max_y) {
    int x0 = col * TILE_SIZE;
    int y0 = row * TILE_SIZE;
    int min_y = INT_MAX;
    int max_y = -1;

    for (int dy = 0; dy < TILE_SIZE; dy++) {
        for (int dx = 0; dx < TILE_SIZE; dx++) {
            int x = x0 + dx;
            int y = y0 + dy;
            if (x < 0 || y < 0 || x >= image->width || y >= image->height)
                continue;
            Color p = GetImageColor(*image, x, y);
            if (p.a > 0) {
                if (y < min_y)
                    min_y = y;
                if (y > max_y)
                    max_y = y;
            }
        }
    }

    if (max_y < 0)
        return false;

    *out_min_y = min_y;
    *out_max_y = max_y;
    return true;
}


bool level_cell_is_solid(const Level *level, int col, int row) {
    if (!level || !level->loaded)
        return false;
    return level_get_tile(level, col, row) != TILE_EMPTY;
}

bool level_has_tag_override(const Level *level, int col, int row) {
    const GameplayCell *cell = gameplay_grid_get(&level->gameplay, col, row);
    return cell && cell->has_override;
}

uint32_t level_cell_effective_flags(const Level *level, const TileCatalog *catalog, int col, int row) {
    if (!level_cell_is_solid(level, col, row))
        return 0;

    const GameplayCell *cell = gameplay_grid_get(&level->gameplay, col, row);
    if (cell && cell->has_override)
        return cell->flags_override;

    int solid_index = tile_catalog_find_type_index(catalog, "solid");
    if (solid_index >= 0) {
        const TileTypeDef *type = tile_catalog_get_type(catalog, solid_index);
        if (type)
            return type->default_flags;
    }
    return TILE_FLAG_COLLISION;
}

void level_sync_from_collision_image(Level *level, const TileCatalog *catalog) {
    if (!level || !catalog || !level->tiles || !level->tile_flags || !level->surface_y)
        return;
    if (!level->collision_edit.data)
        return;

    const Image *image = &level->collision_edit;
    int solid_index = tile_catalog_find_type_index(catalog, "solid");
    const TileTypeDef *solid_type = solid_index >= 0 ? tile_catalog_get_type(catalog, solid_index) : NULL;
    uint32_t default_flags = solid_type ? solid_type->default_flags : TILE_FLAG_COLLISION;

    for (int r = 0; r < level->rows; r++) {
        for (int c = 0; c < level->cols; c++) {
            int idx = r * level->cols + c;
            level->tiles[idx] = TILE_EMPTY;
            level->tile_flags[idx] = 0;
            level->surface_y[idx] = -1;

            int min_y, max_y;
            if (!cell_opaque_bounds(image, c, r, &min_y, &max_y))
                continue;

            level->tiles[idx] = TILE_SOLID;
            level->surface_y[idx] = min_y;

            const GameplayCell *cell = gameplay_grid_get(&level->gameplay, c, r);
            if (cell && cell->has_override)
                level->tile_flags[idx] = cell->flags_override;
            else
                level->tile_flags[idx] = default_flags;
        }
    }
}

void level_collision_paint_cell(Level *level, int col, int row) {
    if (!level || !level->collision_edit.data)
        return;
    if (col < 0 || col >= level->cols || row < 0 || row >= level->rows)
        return;

    ImageDrawRectangle(&level->collision_edit, col * TILE_SIZE, row * TILE_SIZE, TILE_SIZE, TILE_SIZE,
                       (Color){ 0, 0, 0, 255 });
    level->collision_dirty = true;
}

void level_collision_erase_cell(Level *level, int col, int row) {
    if (!level || !level->collision_edit.data)
        return;
    if (col < 0 || col >= level->cols || row < 0 || row >= level->rows)
        return;

    ImageDrawRectangle(&level->collision_edit, col * TILE_SIZE, row * TILE_SIZE, TILE_SIZE, TILE_SIZE,
                       BLANK);
    level->collision_dirty = true;
}

uint32_t level_get_cell_flags(const Level *level, int col, int row) {
    if (!level || !level->loaded || !level->tile_flags)
        return 0;
    if (col < 0 || col >= level->cols || row < 0 || row >= level->rows)
        return 0;
    return level->tile_flags[row * level->cols + col];
}

bool level_cell_has_flag(const Level *level, int col, int row, uint32_t flag) {
    return (level_get_cell_flags(level, col, row) & flag) != 0;
}

bool level_save_gameplay(const Level *level, const TileCatalog *catalog) {
    if (!level || !catalog || !level->loaded || level->gameplay_path[0] == '\0' || !level->act_id)
        return false;

    return gameplay_io_save(&level->gameplay, catalog, level->gameplay_path, TILE_SIZE,
                            level->width, level->height, level->act_id);
}

static bool level_load_image_file(const char *path, int expected_w, int expected_h, Image *out) {
    if (!path || !out)
        return false;

    Image image = LoadImage(path);
    if (image.data == NULL || image.width <= 0 || image.height <= 0) {
        fprintf(stderr, "Failed to load image: %s\n", path);
        return false;
    }

    if (image.width != expected_w || image.height != expected_h) {
        fprintf(stderr, "Layer PNG size mismatch for %s: expected %dx%d, got %dx%d\n",
                path, expected_w, expected_h, image.width, image.height);
        UnloadImage(image);
        return false;
    }

    *out = image;
    return true;
}

static bool level_image_to_texture(const Image *image, Texture2D *out_texture) {
    if (!image || !out_texture)
        return false;

    *out_texture = LoadTextureFromImage(*image);
    if (out_texture->id == 0)
        return false;

    SetTextureFilter(*out_texture, TEXTURE_FILTER_POINT);
    return true;
}

TileType level_get_tile(const Level *level, int col, int row) {
    if (!level || !level->loaded || !level->tiles)
        return TILE_EMPTY;
    if (col < 0 || col >= level->cols || row < 0 || row >= level->rows)
        return TILE_EMPTY;
    return level->tiles[row * level->cols + col];
}

int level_get_surface_y(const Level *level, int col, int row) {
    if (!level || !level->loaded || !level->surface_y)
        return -1;
    if (col < 0 || col >= level->cols || row < 0 || row >= level->rows)
        return -1;
    return level->surface_y[row * level->cols + col];
}

static int level_find_save_slot(const Level *level, int save_index) {
    if (!level || !level->saves)
        return -1;
    for (int i = 0; i < level->save_count; i++) {
        if (level->saves[i].index == save_index)
            return i;
    }
    return -1;
}

bool level_set_active_save(Level *level, int save_index) {
    if (!level || !level->saves || level->save_count <= 0)
        return false;

    int slot = level_find_save_slot(level, save_index);
    if (slot < 0)
        return false;

    const SavePointDef *save = &level->saves[slot];
    level->active_save_index = save_index;
    level->spawn_slice = (Vector2){ save->x, save->y };
    level->spawn = level->spawn_slice;

    if (save->room_id) {
        int room_index = level_find_room_index_by_id(level, save->room_id);
        if (room_index >= 0)
            level->active_room_index = room_index;
    }
    return true;
}

const RoomDef *level_get_active_room(const Level *level) {
    if (!level || !level->loaded || !level->rooms || level->room_count <= 0)
        return NULL;
    if (level->active_room_index < 0 || level->active_room_index >= level->room_count)
        return NULL;
    return &level->rooms[level->active_room_index];
}

const TunnelDef *level_find_tunnel_by_id(const Level *level, const char *tunnel_id) {
    if (!level || !tunnel_id || !level->tunnels)
        return NULL;
    for (int i = 0; i < level->tunnel_count; i++) {
        if (level->tunnels[i].id && strcmp(level->tunnels[i].id, tunnel_id) == 0)
            return &level->tunnels[i];
    }
    return NULL;
}

const TeleportDef *level_find_teleport_by_id(const Level *level, const char *teleport_id) {
    if (!level || !teleport_id || !level->teleports)
        return NULL;
    for (int i = 0; i < level->teleport_count; i++) {
        if (level->teleports[i].id && strcmp(level->teleports[i].id, teleport_id) == 0)
            return &level->teleports[i];
    }
    return NULL;
}

const char *level_tunnel_other_room(const TunnelDef *tunnel, const char *current_room_id) {
    if (!tunnel || !current_room_id)
        return NULL;
    if (tunnel->room_a_id && strcmp(tunnel->room_a_id, current_room_id) == 0)
        return tunnel->room_b_id;
    if (tunnel->room_b_id && strcmp(tunnel->room_b_id, current_room_id) == 0)
        return tunnel->room_a_id;
    return NULL;
}

int level_find_room_index_by_id(const Level *level, const char *room_id) {
    if (!level || !room_id || !level->rooms)
        return -1;
    for (int i = 0; i < level->room_count; i++) {
        if (level->rooms[i].id && strcmp(level->rooms[i].id, room_id) == 0)
            return i;
    }
    return -1;
}

int level_find_room_at(const Level *level, float x, float y) {
    if (!level || !level->rooms)
        return -1;
    for (int i = 0; i < level->room_count; i++) {
        const RoomDef *room = &level->rooms[i];
        if (x >= room->x && x <= room->x + room->w && y >= room->y && y <= room->y + room->h)
            return i;
    }
    return -1;
}

static bool tunnel_spawn_for_room(const TunnelDef *tunnel, const char *room_id, float *spawn_x,
                                  float *spawn_y) {
    if (!tunnel || !room_id || !spawn_x || !spawn_y)
        return false;
    if (tunnel->room_a_id && strcmp(room_id, tunnel->room_a_id) == 0) {
        *spawn_x = tunnel->spawn_ax;
        *spawn_y = tunnel->spawn_ay;
        return true;
    }
    if (tunnel->room_b_id && strcmp(room_id, tunnel->room_b_id) == 0) {
        *spawn_x = tunnel->spawn_bx;
        *spawn_y = tunnel->spawn_by;
        return true;
    }
    return false;
}

bool level_place_at_tunnel(const Level *level, const TunnelDef *tunnel, const char *from_room_id,
                           const char *to_room_id, Vector2 *out_pos) {
    (void)from_room_id;
    (void)level;
    if (!tunnel || !to_room_id || !out_pos)
        return false;

    if (!tunnel_spawn_for_room(tunnel, to_room_id, &out_pos->x, &out_pos->y))
        return false;

    return true;
}

bool level_place_at_teleport(const Level *level, const TeleportDef *to, Vector2 *out_pos) {
    (void)level;
    if (!to || !out_pos)
        return false;

    out_pos->x = to->spawn_x;
    out_pos->y = to->spawn_y;
    return true;
}

static void scan_room_view_from_image(const Image *image, const RoomDef *room,
                                      float *out_y, float *out_h) {
    int rx = (int)room->x;
    int ry = (int)room->y;
    int rw = (int)room->w;
    int rh = (int)room->h;

    if (ry + rh > image->height)
        rh = image->height - ry;
    if (rh <= 0 || rw <= 0) {
        *out_y = room->y;
        *out_h = room->h;
        return;
    }

    int min_y = INT_MAX;
    int max_y = -1;

    for (int y = ry; y < ry + rh; y++) {
        for (int x = rx; x < rx + rw; x++) {
            if (x < 0 || y < 0 || x >= image->width || y >= image->height)
                continue;
            Color p = GetImageColor(*image, x, y);
            if (p.a > 0) {
                if (y < min_y)
                    min_y = y;
                if (y > max_y)
                    max_y = y;
            }
        }
    }

    if (max_y < 0) {
        *out_y = room->y;
        *out_h = room->h;
        return;
    }

    int view_y = min_y - (int)ROOM_VIEW_PADDING_TOP;
    if (view_y < ry)
        view_y = ry;

    int view_h = max_y - view_y + 1;
    int slice_bottom = ry + rh;
    if (view_y + view_h > slice_bottom)
        view_h = slice_bottom - view_y;
    if (view_y + view_h > image->height)
        view_h = image->height - view_y;
    if (view_h <= 0) {
        *out_y = room->y;
        *out_h = room->h;
        return;
    }

    *out_y = (float)view_y;
    *out_h = (float)view_h;
}

static bool level_alloc_room_views(Level *level) {
    if (level->room_count <= 0)
        return true;

    level->room_view_y = calloc((size_t)level->room_count, sizeof(float));
    level->room_view_h = calloc((size_t)level->room_count, sizeof(float));
    return level->room_view_y && level->room_view_h;
}

static void level_build_room_views(Level *level, const Image *image) {
    if (!level->rooms || !level->room_view_y || !level->room_view_h)
        return;

    for (int i = 0; i < level->room_count; i++)
        scan_room_view_from_image(image, &level->rooms[i], &level->room_view_y[i],
                                  &level->room_view_h[i]);
}

void level_refresh_collision_texture(Level *level, const TileCatalog *catalog) {
    if (!level || !level->collision_edit.data)
        return;

    if (level->tex_base.id != 0)
        UnloadTexture(level->tex_base);

    if (!level_image_to_texture(&level->collision_edit, &level->tex_base)) {
        TraceLog(LOG_WARNING, "level: failed to refresh collision texture");
        return;
    }

    level_sync_from_collision_image(level, catalog);
    level_build_room_views(level, &level->collision_edit);
}

bool level_save_collision_to_aseprite(Level *level) {
    if (!level || !level->collision_edit.data || level->aseprite_path[0] == '\0')
        return false;

    if (!act_export_save_collision(level->aseprite_path, &level->collision_edit))
        return false;

    level->collision_dirty = false;
    return true;
}

bool level_reload_visuals(Level *level, const ActDesc *act) {
    if (!level || !act)
        return false;

    const TileCatalog *catalog = tile_catalog_global();
    if (!catalog)
        return false;

    if (level->collision_edit.data)
        UnloadImage(level->collision_edit);

    Image collision = { 0 };
    if (!level_load_image_file(act->collision_png, act->width, act->height, &collision)) {
        TraceLog(LOG_WARNING, "level_reload: collision PNG missing: %s", act->collision_png);
        collision = GenImageColor(act->width, act->height, BLANK);
    }
    level->collision_edit = collision;
    snprintf(level->collision_png_path, sizeof(level->collision_png_path), "%s", act->collision_png);

    if (level->tex_base.id != 0)
        UnloadTexture(level->tex_base);
    if (!level_image_to_texture(&level->collision_edit, &level->tex_base))
        return false;

    level_sync_from_collision_image(level, catalog);
    level_build_room_views(level, &level->collision_edit);

    if (level->tex_background.id != 0)
        UnloadTexture(level->tex_background);

    Image background = { 0 };
    if (level_load_image_file(act->background_png, act->width, act->height, &background)) {
        if (!level_image_to_texture(&background, &level->tex_background))
            fprintf(stderr, "Warning: failed to reload background texture\n");
        UnloadImage(background);
    }

    return true;
}

float level_room_view_y(const Level *level, int room_index) {
    if (!level || !level->rooms || room_index < 0 || room_index >= level->room_count)
        return 0.0f;
    if (level->room_view_y && level->room_view_h[room_index] > 0.0f)
        return level->room_view_y[room_index];
    return level->rooms[room_index].y;
}

float level_room_view_h(const Level *level, int room_index) {
    if (!level || !level->rooms || room_index < 0 || room_index >= level->room_count)
        return 0.0f;
    if (level->room_view_h && level->room_view_h[room_index] > 0.0f)
        return level->room_view_h[room_index];
    return level->rooms[room_index].h;
}

float level_active_view_y(const Level *level) {
    return level_room_view_y(level, level ? level->active_room_index : -1);
}

float level_active_view_h(const Level *level) {
    return level_room_view_h(level, level ? level->active_room_index : -1);
}

static bool point_in_rect(float x, float y, float rx, float ry, float rw, float rh) {
    return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

LevelZone level_zone_at(const Level *level, float x, float y) {
    LevelZone zone = { ZONE_NONE, -1, NULL };
    if (!level || !level->loaded)
        return zone;

    if (level->tunnels) {
        for (int i = 0; i < level->tunnel_count; i++) {
            const TunnelDef *tunnel = &level->tunnels[i];
            if (point_in_rect(x, y, tunnel->x, tunnel->y, tunnel->w, tunnel->h)) {
                zone.kind = ZONE_TUNNEL;
                zone.index = i;
                zone.id = tunnel->id;
                return zone;
            }
        }
    }

    int room_index = level_find_room_at(level, x, y);
    if (room_index >= 0) {
        zone.kind = ZONE_ROOM;
        zone.index = room_index;
        zone.id = level->rooms[room_index].id;
    }
    return zone;
}

LevelZone level_get_active_zone(const Level *level) {
    LevelZone zone = { ZONE_NONE, -1, NULL };
    if (!level || !level->loaded || !level->rooms)
        return zone;
    if (level->active_room_index < 0 || level->active_room_index >= level->room_count)
        return zone;

    zone.kind = ZONE_ROOM;
    zone.index = level->active_room_index;
    zone.id = level->rooms[level->active_room_index].id;
    return zone;
}

bool level_load(Level *level, const ActDesc *act) {
    if (!level || !act)
        return false;

    level_free(level);
    memset(level, 0, sizeof(*level));

    level->width = act->width;
    level->height = act->height;
    level->cols = act->cols;
    level->rows = act->rows;
    level->rooms = act->rooms;
    level->room_count = act->room_count;
    level->tunnels = act->tunnels;
    level->tunnel_count = act->tunnel_count;
    level->teleports = act->teleports;
    level->teleport_count = act->teleport_count;
    level->saves = act->saves;
    level->save_count = act->save_count;
    level->active_save_index = 0;
    level->active_room_index = 0;

    if (!level_alloc_room_views(level)) {
        fprintf(stderr, "Failed to allocate room views for %s\n", act->id);
        level_free(level);
        return false;
    }

    if (level->cols <= 0 || level->rows <= 0) {
        fprintf(stderr, "Invalid act dimensions for %s\n", act->id);
        return false;
    }

    level->act_id = act->id;
    snprintf(level->aseprite_path, sizeof(level->aseprite_path), "%s", act->aseprite_path);
    snprintf(level->collision_png_path, sizeof(level->collision_png_path), "%s", act->collision_png);
    if (act->gameplay_json[0]) {
        snprintf(level->gameplay_path, sizeof(level->gameplay_path), "%s", act->gameplay_json);
    } else {
        snprintf(level->gameplay_path, sizeof(level->gameplay_path),
                 "resources/visual/layers/%s.gameplay.json", act->id);
    }

    size_t cell_count = (size_t)level->cols * (size_t)level->rows;
    level->tiles = calloc(cell_count, sizeof(TileType));
    level->tile_flags = calloc(cell_count, sizeof(uint32_t));
    level->surface_y = calloc(cell_count, sizeof(int));
    if (!level->tiles || !level->tile_flags || !level->surface_y) {
        fprintf(stderr, "Failed to allocate tile grid for %s\n", act->id);
        level_free(level);
        return false;
    }

    for (size_t i = 0; i < cell_count; i++)
        level->surface_y[i] = -1;

    Image base_image = { 0 };
    if (!level_load_image_file(act->collision_png, act->width, act->height, &base_image)) {
        TraceLog(LOG_WARNING, "Collision PNG missing, using empty: %s", act->collision_png);
        base_image = GenImageColor(act->width, act->height, BLANK);
    }

    const TileCatalog *catalog = tile_catalog_global();
    if (!catalog) {
        fprintf(stderr, "Tile catalog not loaded\n");
        UnloadImage(base_image);
        level_free(level);
        return false;
    }

    level->collision_edit = base_image;

    if (!gameplay_grid_init(&level->gameplay, level->cols, level->rows)) {
        level_free(level);
        return false;
    }

    if (file_exists(level->gameplay_path)) {
        if (!gameplay_io_load(&level->gameplay, catalog, level->gameplay_path, level->cols,
                              level->rows, act->id))
            fprintf(stderr, "Warning: failed to load gameplay json: %s\n", level->gameplay_path);
    }

    level_sync_from_collision_image(level, catalog);
    level_build_room_views(level, &level->collision_edit);
    if (!level_image_to_texture(&level->collision_edit, &level->tex_base)) {
        level_free(level);
        return false;
    }

    Image background_image = { 0 };
    if (level_load_image_file(act->background_png, act->width, act->height, &background_image)) {
        if (!level_image_to_texture(&background_image, &level->tex_background))
            fprintf(stderr, "Warning: failed to upload background texture\n");
        UnloadImage(background_image);
    } else {
        fprintf(stderr, "Warning: background not found: %s\n", act->background_png);
    }

    level->spawn_slice = (Vector2){ 0.0f, 0.0f };
    level->spawn = (Vector2){ 0.0f, 0.0f };
    if (!level_set_active_save(level, 0)) {
        fprintf(stderr, "Failed to apply save-0 for %s\n", act->id);
        level_free(level);
        return false;
    }

    level->loaded = true;
    return true;
}

void level_free(Level *level) {
    if (!level)
        return;
    if (level->tex_background.id != 0)
        UnloadTexture(level->tex_background);
    if (level->tex_base.id != 0)
        UnloadTexture(level->tex_base);
    if (level->collision_edit.data)
        UnloadImage(level->collision_edit);
    free(level->tiles);
    free(level->tile_flags);
    free(level->surface_y);
    free(level->room_view_y);
    free(level->room_view_h);
    gameplay_grid_free(&level->gameplay);
    memset(level, 0, sizeof(*level));
}
