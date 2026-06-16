#include "tile_config.h"

const TileDef TILE_DEFS[TILE_COUNT] = {
    [TILE_EMPTY] = { false, (Color){ 0, 0, 0, 0 } },
    [TILE_SOLID] = { true,  (Color){ 120, 90, 60, 255 } },
};
