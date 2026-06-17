#include "gameplay_io.h"

#include "../../external/cjson/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool read_file_text(const char *path, char **out_text) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

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
    *out_text = text;
    return true;
}

static uint32_t parse_flags_field(const TileCatalog *catalog, cJSON *flags_json) {
    if (!flags_json || cJSON_IsNull(flags_json))
        return UINT32_MAX;

    if (!cJSON_IsArray(flags_json))
        return 0;

    uint32_t mask = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, flags_json) {
        if (!cJSON_IsString(item) || !item->valuestring)
            continue;
        int fi = tile_catalog_find_flag_index(catalog, item->valuestring);
        if (fi >= 0)
            mask |= catalog->flags[fi].bit;
    }
    return mask;
}

bool gameplay_io_load(GameplayGrid *grid, const TileCatalog *catalog, const char *path,
                      int expected_cols, int expected_rows, const char *act_id) {
    if (!grid || !catalog || !path)
        return false;

    char *text = NULL;
    if (!read_file_text(path, &text))
        return false;

    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) {
        fprintf(stderr, "gameplay_io: parse error in %s\n", path);
        return false;
    }

    cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (act_id && cJSON_IsString(id) && id->valuestring && strcmp(id->valuestring, act_id) != 0) {
        fprintf(stderr, "gameplay_io: id mismatch in %s (expected %s, got %s)\n",
                path, act_id, id->valuestring);
        cJSON_Delete(root);
        return false;
    }

    if (!gameplay_grid_init(grid, expected_cols, expected_rows)) {
        cJSON_Delete(root);
        return false;
    }

    int solid_index = tile_catalog_find_type_index(catalog, "solid");
    if (solid_index < 0)
        solid_index = 0;

    cJSON *cells = cJSON_GetObjectItemCaseSensitive(root, "cells");
    if (!cJSON_IsArray(cells)) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *cell = NULL;
    cJSON_ArrayForEach(cell, cells) {
        cJSON *c = cJSON_GetObjectItemCaseSensitive(cell, "c");
        cJSON *r = cJSON_GetObjectItemCaseSensitive(cell, "r");
        cJSON *type = cJSON_GetObjectItemCaseSensitive(cell, "type");
        cJSON *flags = cJSON_GetObjectItemCaseSensitive(cell, "flags");

        if (!cJSON_IsNumber(c) || !cJSON_IsNumber(r))
            continue;

        int col = c->valueint;
        int row = r->valueint;
        if (!gameplay_grid_in_bounds(grid, col, row))
            continue;

        uint32_t mask = UINT32_MAX;
        if (flags)
            mask = parse_flags_field(catalog, flags);

        if (mask == UINT32_MAX) {
            if (!cJSON_IsString(type) || !type->valuestring)
                continue;
            int type_index = tile_catalog_find_type_index(catalog, type->valuestring);
            if (type_index < 0)
                continue;
            const TileTypeDef *type_def = tile_catalog_get_type(catalog, type_index);
            if (!type_def)
                continue;
            mask = type_def->default_flags;
        }

        GameplayCell *dst = gameplay_grid_get_mut(grid, col, row);
        if (!dst)
            continue;

        dst->type_index = (int16_t)solid_index;
        dst->has_override = true;
        dst->flags_override = mask;
    }

    cJSON_Delete(root);
    return true;
}

bool gameplay_io_save(const GameplayGrid *grid, const TileCatalog *catalog, const char *path,
                      int tile_size, int width, int height, const char *act_id) {
    if (!grid || !catalog || !path || !act_id)
        return false;

    cJSON *root = cJSON_CreateObject();
    cJSON *cells = cJSON_CreateArray();
    if (!root || !cells) {
        cJSON_Delete(root);
        cJSON_Delete(cells);
        return false;
    }

    cJSON_AddStringToObject(root, "id", act_id);
    cJSON_AddNumberToObject(root, "tile_size", tile_size);
    cJSON_AddNumberToObject(root, "width", width);
    cJSON_AddNumberToObject(root, "height", height);
    cJSON_AddItemToObject(root, "cells", cells);

    for (int row = 0; row < grid->rows; row++) {
        for (int col = 0; col < grid->cols; col++) {
            const GameplayCell *cell = gameplay_grid_get(grid, col, row);
            if (!cell || !cell->has_override)
                continue;

            cJSON *entry = cJSON_CreateObject();
            cJSON_AddNumberToObject(entry, "c", col);
            cJSON_AddNumberToObject(entry, "r", row);

            cJSON *flags_arr = cJSON_CreateArray();
            for (int fi = 0; fi < catalog->flag_count; fi++) {
                if (cell->flags_override & catalog->flags[fi].bit)
                    cJSON_AddItemToArray(flags_arr, cJSON_CreateString(catalog->flags[fi].id));
            }
            cJSON_AddItemToObject(entry, "flags", flags_arr);
            cJSON_AddItemToArray(cells, entry);
        }
    }

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text)
        return false;

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(text);
        fprintf(stderr, "gameplay_io: cannot write %s\n", path);
        return false;
    }

    fputs(text, f);
    fputc('\n', f);
    fclose(f);
    free(text);
    return true;
}
