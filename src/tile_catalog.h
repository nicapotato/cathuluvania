#ifndef TILE_CATALOG_H
#define TILE_CATALOG_H

#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>

#define TILE_CATALOG_MAX_FLAGS 16
#define TILE_CATALOG_MAX_TYPES 32
#define TILE_CATALOG_ID_LEN 32
#define TILE_CATALOG_LABEL_LEN 48

typedef struct TileFlagDef {
    char id[TILE_CATALOG_ID_LEN];
    char label[TILE_CATALOG_LABEL_LEN];
    uint32_t bit;
} TileFlagDef;

typedef struct TileTypeDef {
    char id[TILE_CATALOG_ID_LEN];
    char label[TILE_CATALOG_LABEL_LEN];
    Color color;
    uint32_t default_flags;
    int index;
} TileTypeDef;

typedef struct TileCatalog {
    TileFlagDef flags[TILE_CATALOG_MAX_FLAGS];
    int flag_count;
    TileTypeDef types[TILE_CATALOG_MAX_TYPES];
    int type_count;
    bool loaded;
} TileCatalog;

bool tile_catalog_load(TileCatalog *catalog, const char *path);
void tile_catalog_free(TileCatalog *catalog);

const TileCatalog *tile_catalog_global(void);
bool tile_catalog_load_global(const char *path);

int tile_catalog_find_type_index(const TileCatalog *catalog, const char *id);
int tile_catalog_find_flag_index(const TileCatalog *catalog, const char *id);
uint32_t tile_catalog_flags_from_names(const TileCatalog *catalog, const char **names, int count);
bool tile_catalog_flag_names_from_mask(const TileCatalog *catalog, uint32_t mask,
                                       const char **out_names, int out_cap, int *out_count);

const TileTypeDef *tile_catalog_get_type(const TileCatalog *catalog, int index);
const TileFlagDef *tile_catalog_get_flag(const TileCatalog *catalog, int index);

#endif
