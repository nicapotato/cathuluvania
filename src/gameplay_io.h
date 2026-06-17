#ifndef GAMEPLAY_IO_H
#define GAMEPLAY_IO_H

#include "gameplay_grid.h"
#include "tile_catalog.h"
#include <stdbool.h>

bool gameplay_io_load(GameplayGrid *grid, const TileCatalog *catalog, const char *path,
                      int expected_cols, int expected_rows, const char *act_id);
bool gameplay_io_save(const GameplayGrid *grid, const TileCatalog *catalog, const char *path,
                      int tile_size, int width, int height, const char *act_id);

#endif
