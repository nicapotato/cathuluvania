#ifndef TILE_CONFIG_H
#define TILE_CONFIG_H

#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TILE_EMPTY = 0,
    TILE_SOLID,
    TILE_COUNT
} TileType;

#define TILE_FLAG_COLLISION (1u << 0)
#define TILE_FLAG_SLIPPERY  (1u << 1)
#define TILE_FLAG_CLIMB     (1u << 2)

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

static inline bool tile_flags_has_collision(uint32_t flags) {
    return (flags & TILE_FLAG_COLLISION) != 0;
}

#endif
