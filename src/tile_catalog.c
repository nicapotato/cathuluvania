#include "tile_catalog.h"

#include "../../external/cjson/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TileCatalog g_global_catalog;

static Color parse_hex_color(const char *hex) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7)
        return (Color){ 200, 200, 200, 255 };

    unsigned int r = 0, g = 0, b = 0;
    if (sscanf(hex + 1, "%2x%2x%2x", &r, &g, &b) != 3)
        return (Color){ 200, 200, 200, 255 };

    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
}

static void copy_str(char *dst, int cap, const char *src) {
    if (!dst || cap <= 0)
        return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, (size_t)cap, "%s", src);
}

static uint32_t parse_flag_array(const TileCatalog *catalog, cJSON *arr) {
    uint32_t mask = 0;
    if (!cJSON_IsArray(arr))
        return 0;

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, arr) {
        if (!cJSON_IsString(item) || !item->valuestring)
            continue;
        int fi = tile_catalog_find_flag_index(catalog, item->valuestring);
        if (fi >= 0)
            mask |= catalog->flags[fi].bit;
    }
    return mask;
}

void tile_catalog_free(TileCatalog *catalog) {
    if (!catalog)
        return;
    memset(catalog, 0, sizeof(*catalog));
}

bool tile_catalog_load(TileCatalog *catalog, const char *path) {
    if (!catalog || !path)
        return false;

    tile_catalog_free(catalog);

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "tile_catalog: cannot open %s\n", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        fclose(f);
        return false;
    }

    char *text = (char *)malloc((size_t)len + 1);
    if (!text) {
        fclose(f);
        return false;
    }

    if (fread(text, 1, (size_t)len, f) != (size_t)len) {
        free(text);
        fclose(f);
        return false;
    }
    text[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) {
        fprintf(stderr, "tile_catalog: parse error in %s\n", path);
        return false;
    }

    cJSON *flags_json = cJSON_GetObjectItemCaseSensitive(root, "flags");
    if (cJSON_IsArray(flags_json)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, flags_json) {
            if (catalog->flag_count >= TILE_CATALOG_MAX_FLAGS)
                break;
            cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
            cJSON *label = cJSON_GetObjectItemCaseSensitive(item, "label");
            if (!cJSON_IsString(id) || !id->valuestring)
                continue;

            TileFlagDef *fd = &catalog->flags[catalog->flag_count];
            copy_str(fd->id, TILE_CATALOG_ID_LEN, id->valuestring);
            copy_str(fd->label, TILE_CATALOG_LABEL_LEN,
                     cJSON_IsString(label) && label->valuestring ? label->valuestring : fd->id);
            fd->bit = (1u << catalog->flag_count);
            catalog->flag_count++;
        }
    }

    cJSON *types_json = cJSON_GetObjectItemCaseSensitive(root, "types");
    if (cJSON_IsArray(types_json)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, types_json) {
            if (catalog->type_count >= TILE_CATALOG_MAX_TYPES)
                break;
            cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
            cJSON *label = cJSON_GetObjectItemCaseSensitive(item, "label");
            cJSON *color = cJSON_GetObjectItemCaseSensitive(item, "color");
            cJSON *flags = cJSON_GetObjectItemCaseSensitive(item, "flags");
            if (!cJSON_IsString(id) || !id->valuestring)
                continue;

            TileTypeDef *td = &catalog->types[catalog->type_count];
            td->index = catalog->type_count;
            copy_str(td->id, TILE_CATALOG_ID_LEN, id->valuestring);
            copy_str(td->label, TILE_CATALOG_LABEL_LEN,
                     cJSON_IsString(label) && label->valuestring ? label->valuestring : td->id);
            td->color = parse_hex_color(cJSON_IsString(color) ? color->valuestring : "#FFFFFF");
            td->default_flags = parse_flag_array(catalog, flags);
            catalog->type_count++;
        }
    }

    cJSON_Delete(root);

    if (catalog->type_count == 0) {
        fprintf(stderr, "tile_catalog: no types in %s\n", path);
        return false;
    }

    catalog->loaded = true;
    return true;
}

const TileCatalog *tile_catalog_global(void) {
    return g_global_catalog.loaded ? &g_global_catalog : NULL;
}

bool tile_catalog_load_global(const char *path) {
    return tile_catalog_load(&g_global_catalog, path);
}

int tile_catalog_find_type_index(const TileCatalog *catalog, const char *id) {
    if (!catalog || !id)
        return -1;
    for (int i = 0; i < catalog->type_count; i++) {
        if (strcmp(catalog->types[i].id, id) == 0)
            return i;
    }
    return -1;
}

int tile_catalog_find_flag_index(const TileCatalog *catalog, const char *id) {
    if (!catalog || !id)
        return -1;
    for (int i = 0; i < catalog->flag_count; i++) {
        if (strcmp(catalog->flags[i].id, id) == 0)
            return i;
    }
    return -1;
}

uint32_t tile_catalog_flags_from_names(const TileCatalog *catalog, const char **names, int count) {
    uint32_t mask = 0;
    if (!catalog || !names)
        return 0;
    for (int i = 0; i < count; i++) {
        int fi = tile_catalog_find_flag_index(catalog, names[i]);
        if (fi >= 0)
            mask |= catalog->flags[fi].bit;
    }
    return mask;
}

bool tile_catalog_flag_names_from_mask(const TileCatalog *catalog, uint32_t mask,
                                       const char **out_names, int out_cap, int *out_count) {
    if (!catalog || !out_names || !out_count || out_cap <= 0)
        return false;

    int n = 0;
    for (int i = 0; i < catalog->flag_count; i++) {
        if ((mask & catalog->flags[i].bit) == 0)
            continue;
        if (n >= out_cap)
            return false;
        out_names[n++] = catalog->flags[i].id;
    }
    *out_count = n;
    return true;
}

const TileTypeDef *tile_catalog_get_type(const TileCatalog *catalog, int index) {
    if (!catalog || index < 0 || index >= catalog->type_count)
        return NULL;
    return &catalog->types[index];
}

const TileFlagDef *tile_catalog_get_flag(const TileCatalog *catalog, int index) {
    if (!catalog || index < 0 || index >= catalog->flag_count)
        return NULL;
    return &catalog->flags[index];
}
