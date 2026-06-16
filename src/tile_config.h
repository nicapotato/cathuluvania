#ifndef TILE_CONFIG_H
#define TILE_CONFIG_H

#include "raylib.h"
#include <stdbool.h>

typedef enum {
    TILE_EMPTY = 0,
    TILE_SOLID,
    TILE_COUNT
} TileType;

typedef struct {
    bool is_solid;
    Color fallback_color;
} TileDef;

extern const TileDef TILE_DEFS[TILE_COUNT];

static inline bool tile_is_solid(TileType t) {
    if (t < 0 || t >= TILE_COUNT)
        return false;
    return TILE_DEFS[t].is_solid;
}

#endif
