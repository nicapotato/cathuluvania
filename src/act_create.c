#include "act_create.h"

#include "act_export.h"
#include "../../external/cjson/cJSON.h"
#include "gameplay_io.h"
#include "main.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ACT_MANIFEST_PATH "resources/acts.manifest.json"
#define EXPORT_DIR "resources/visual/layers"

static bool file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0;
}

static bool copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in)
        return false;

    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return false;
        }
    }

    fclose(in);
    fclose(out);
    return true;
}

static bool manifest_has_id(const char *id) {
    char *text = NULL;
    FILE *f = fopen(ACT_MANIFEST_PATH, "rb");
    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        fclose(f);
        return false;
    }

    text = (char *)malloc((size_t)len + 1);
    if (!text) {
        fclose(f);
        return false;
    }

    fread(text, 1, (size_t)len, f);
    text[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root)
        return false;

    bool found = false;
    cJSON *acts = cJSON_GetObjectItemCaseSensitive(root, "acts");
    if (cJSON_IsArray(acts)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, acts) {
            cJSON *jid = cJSON_GetObjectItemCaseSensitive(item, "id");
            if (cJSON_IsString(jid) && jid->valuestring && strcmp(jid->valuestring, id) == 0) {
                found = true;
                break;
            }
        }
    }

    cJSON_Delete(root);
    return found;
}

bool act_create_generate_id(char *out_id, int out_cap) {
    if (!out_id || out_cap <= 0)
        return false;

    for (int n = 1; n < 1000; n++) {
        snprintf(out_id, (size_t)out_cap, "new-act-%03d", n);
        if (!manifest_has_id(out_id) && !file_exists(TextFormat("resources/visual/%s.aseprite", out_id)))
            return true;
    }

    return false;
}

static bool read_export_dimensions(const char *id, int *out_width, int *out_height) {
    if (!id || !out_width || !out_height)
        return false;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s.export.json", EXPORT_DIR, id);

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

    fread(text, 1, (size_t)len, f);
    text[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root)
        return false;

    cJSON *width = cJSON_GetObjectItemCaseSensitive(root, "width");
    cJSON *height = cJSON_GetObjectItemCaseSensitive(root, "height");
    bool ok = cJSON_IsNumber(width) && cJSON_IsNumber(height);
    if (ok) {
        *out_width = width->valueint;
        *out_height = height->valueint;
    }

    cJSON_Delete(root);
    return ok;
}

static bool write_empty_gameplay_json(const char *id, int width, int height) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.gameplay.json", EXPORT_DIR, id);

    cJSON *root = cJSON_CreateObject();
    cJSON *cells = cJSON_CreateArray();
    if (!root || !cells) {
        cJSON_Delete(root);
        return false;
    }

    cJSON_AddStringToObject(root, "id", id);
    cJSON_AddNumberToObject(root, "tile_size", TILE_SIZE);
    cJSON_AddNumberToObject(root, "width", width);
    cJSON_AddNumberToObject(root, "height", height);
    cJSON_AddItemToObject(root, "cells", cells);

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text)
        return false;

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(text);
        return false;
    }

    fputs(text, f);
    fputc('\n', f);
    fclose(f);
    free(text);
    return true;
}

bool act_create_from_template(const char *id, const char *label) {
    if (!id || !label || id[0] == '\0')
        return false;

    if (!file_exists(ACT_TEMPLATE_ASEPRITE)) {
        TraceLog(LOG_WARNING, "act_create: template missing: %s", ACT_TEMPLATE_ASEPRITE);
        return false;
    }

    char dst_ase[256];
    snprintf(dst_ase, sizeof(dst_ase), "resources/visual/%s.aseprite", id);

    if (!copy_file(ACT_TEMPLATE_ASEPRITE, dst_ase)) {
        TraceLog(LOG_WARNING, "act_create: failed to copy template to %s", dst_ase);
        return false;
    }

    if (!act_export_run_level_export(dst_ase)) {
        TraceLog(LOG_WARNING, "act_create: export failed for %s", dst_ase);
        return false;
    }

    int width = 0;
    int height = 0;
    if (!read_export_dimensions(id, &width, &height)) {
        TraceLog(LOG_WARNING, "act_create: failed to read export dimensions for %s", id);
        return false;
    }

    if (!write_empty_gameplay_json(id, width, height))
        return false;

    if (!manifest_has_id(id) && !act_registry_append_manifest_entry(ACT_MANIFEST_PATH, id, label)) {
        TraceLog(LOG_WARNING, "act_create: failed to update manifest");
        return false;
    }

    TraceLog(LOG_INFO, "act_create: created %s from template (%dx%d)", id, width, height);
    return true;
}
