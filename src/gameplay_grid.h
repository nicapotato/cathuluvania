#ifndef GAMEPLAY_GRID_H
#define GAMEPLAY_GRID_H

#include "tile_catalog.h"
#include "tile_config.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct GameplayCell {
    int16_t type_index;
    uint32_t flags_override;
    bool has_override;
} GameplayCell;

typedef struct GameplayGrid {
    int cols;
    int rows;
    GameplayCell *cells;
    bool loaded;
} GameplayGrid;

bool gameplay_grid_init(GameplayGrid *grid, int cols, int rows);
void gameplay_grid_free(GameplayGrid *grid);

bool gameplay_grid_in_bounds(const GameplayGrid *grid, int col, int row);
int gameplay_grid_index(const GameplayGrid *grid, int col, int row);

const GameplayCell *gameplay_grid_get(const GameplayGrid *grid, int col, int row);
GameplayCell *gameplay_grid_get_mut(GameplayGrid *grid, int col, int row);

bool gameplay_grid_has_cell(const GameplayGrid *grid, int col, int row);
uint32_t gameplay_grid_effective_flags(const GameplayGrid *grid, const TileCatalog *catalog,
                                       int col, int row);

void gameplay_grid_set_type(GameplayGrid *grid, int col, int row, int type_index);
void gameplay_grid_clear_cell(GameplayGrid *grid, int col, int row);
void gameplay_grid_set_flags_override(GameplayGrid *grid, const TileCatalog *catalog,
                                      int col, int row, uint32_t flags);
void gameplay_grid_clear_flags_override(GameplayGrid *grid, int col, int row);
void gameplay_grid_toggle_flag(GameplayGrid *grid, const TileCatalog *catalog,
                               int col, int row, uint32_t flag_bit);

void gameplay_grid_apply_to_level(const GameplayGrid *grid, const TileCatalog *catalog,
                                  int cols, int rows, TileType *tiles, uint32_t *tile_flags,
                                  int *surface_y);

bool gameplay_grid_migrate_from_image(GameplayGrid *grid, const TileCatalog *catalog,
                                      const Image *image, int cols, int rows);

#endif
