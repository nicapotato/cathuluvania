#include "gameplay_grid.h"

#include "main.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool gameplay_grid_init(GameplayGrid *grid, int cols, int rows) {
    if (!grid || cols <= 0 || rows <= 0)
        return false;

    gameplay_grid_free(grid);

    size_t count = (size_t)cols * (size_t)rows;
    grid->cells = (GameplayCell *)calloc(count, sizeof(GameplayCell));
    if (!grid->cells)
        return false;

    grid->cols = cols;
    grid->rows = rows;
    grid->loaded = true;
    return true;
}

void gameplay_grid_free(GameplayGrid *grid) {
    if (!grid)
        return;
    free(grid->cells);
    memset(grid, 0, sizeof(*grid));
}

bool gameplay_grid_in_bounds(const GameplayGrid *grid, int col, int row) {
    return grid && col >= 0 && col < grid->cols && row >= 0 && row < grid->rows;
}

int gameplay_grid_index(const GameplayGrid *grid, int col, int row) {
    if (!gameplay_grid_in_bounds(grid, col, row))
        return -1;
    return row * grid->cols + col;
}

const GameplayCell *gameplay_grid_get(const GameplayGrid *grid, int col, int row) {
    int idx = gameplay_grid_index(grid, col, row);
    if (idx < 0)
        return NULL;
    return &grid->cells[idx];
}

GameplayCell *gameplay_grid_get_mut(GameplayGrid *grid, int col, int row) {
    int idx = gameplay_grid_index(grid, col, row);
    if (idx < 0)
        return NULL;
    return &grid->cells[idx];
}

bool gameplay_grid_has_cell(const GameplayGrid *grid, int col, int row) {
    const GameplayCell *cell = gameplay_grid_get(grid, col, row);
    return cell && cell->type_index >= 0;
}

uint32_t gameplay_grid_effective_flags(const GameplayGrid *grid, const TileCatalog *catalog,
                                     int col, int row) {
    const GameplayCell *cell = gameplay_grid_get(grid, col, row);
    if (!cell || cell->type_index < 0 || !catalog)
        return 0;

    if (cell->has_override)
        return cell->flags_override;

    const TileTypeDef *type = tile_catalog_get_type(catalog, cell->type_index);
    return type ? type->default_flags : 0;
}

void gameplay_grid_set_type(GameplayGrid *grid, int col, int row, int type_index) {
    GameplayCell *cell = gameplay_grid_get_mut(grid, col, row);
    if (!cell)
        return;
    cell->type_index = (int16_t)type_index;
    cell->has_override = false;
    cell->flags_override = 0;
}

void gameplay_grid_clear_cell(GameplayGrid *grid, int col, int row) {
    GameplayCell *cell = gameplay_grid_get_mut(grid, col, row);
    if (!cell)
        return;
    cell->type_index = -1;
    cell->has_override = false;
    cell->flags_override = 0;
}

void gameplay_grid_set_flags_override(GameplayGrid *grid, const TileCatalog *catalog,
                                      int col, int row, uint32_t flags) {
    GameplayCell *cell = gameplay_grid_get_mut(grid, col, row);
    if (!cell || cell->type_index < 0)
        return;
    (void)catalog;
    cell->has_override = true;
    cell->flags_override = flags;
}

void gameplay_grid_clear_flags_override(GameplayGrid *grid, int col, int row) {
    GameplayCell *cell = gameplay_grid_get_mut(grid, col, row);
    if (!cell)
        return;
    cell->has_override = false;
    cell->flags_override = 0;
}

void gameplay_grid_toggle_flag(GameplayGrid *grid, const TileCatalog *catalog,
                               int col, int row, uint32_t flag_bit) {
    GameplayCell *cell = gameplay_grid_get_mut(grid, col, row);
    if (!cell || cell->type_index < 0 || !catalog)
        return;

    uint32_t flags = gameplay_grid_effective_flags(grid, catalog, col, row);
    if (flags & flag_bit)
        flags &= ~flag_bit;
    else
        flags |= flag_bit;

    cell->has_override = true;
    cell->flags_override = flags;
}

void gameplay_grid_apply_to_level(const GameplayGrid *grid, const TileCatalog *catalog,
                                  int cols, int rows, TileType *tiles, uint32_t *tile_flags,
                                  int *surface_y) {
    if (!grid || !catalog || !tiles || !tile_flags || !surface_y)
        return;

    int use_cols = grid->cols < cols ? grid->cols : cols;
    int use_rows = grid->rows < rows ? grid->rows : rows;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int idx = r * cols + c;
            tiles[idx] = TILE_EMPTY;
            tile_flags[idx] = 0;
            surface_y[idx] = -1;
        }
    }

    for (int r = 0; r < use_rows; r++) {
        for (int c = 0; c < use_cols; c++) {
            if (!gameplay_grid_has_cell(grid, c, r))
                continue;

            int idx = r * cols + c;
            uint32_t flags = gameplay_grid_effective_flags(grid, catalog, c, r);
            tile_flags[idx] = flags;

            if (tile_flags_has_collision(flags)) {
                tiles[idx] = TILE_SOLID;
                surface_y[idx] = r * TILE_SIZE;
            }
        }
    }
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

bool gameplay_grid_migrate_from_image(GameplayGrid *grid, const TileCatalog *catalog,
                                      const Image *image, int cols, int rows) {
    if (!grid || !catalog || !image || cols <= 0 || rows <= 0)
        return false;

    if (!gameplay_grid_init(grid, cols, rows))
        return false;

    int solid_index = tile_catalog_find_type_index(catalog, "solid");
    if (solid_index < 0)
        solid_index = 0;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int min_y, max_y;
            if (cell_opaque_bounds(image, c, r, &min_y, &max_y))
                gameplay_grid_set_type(grid, c, r, solid_index);
        }
    }

    return true;
}
