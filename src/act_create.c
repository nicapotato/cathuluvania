#include "act_create.h"

#include "../../external/cjson/cJSON.h"
#include "gameplay_io.h"
#include "main.h"
#include "raylib.h"
#include "tile_catalog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ACT_MANIFEST_PATH "resources/acts.manifest.json"
#define EXPORT_DIR "resources/visual/layers"
#define TILESET_TEMPLATE "resources/visual/green-act.aseprite.bk"
#define CREATE_ACT_SCRIPT "scripts/aesprite/create-act.lua"

static bool file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0;
}

static bool ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return true;
    return mkdir(path, 0755) == 0 || (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static bool write_png_solid(const char *path, int width, int height, Color color) {
    Image img = GenImageColor(width, height, color);
    if (!img.data)
        return false;

    bool ok = ExportImage(img, path);
    UnloadImage(img);
    return ok;
}

static bool write_png_transparent(const char *path, int width, int height) {
    Image img = GenImageColor(width, height, BLANK);
    if (!img.data)
        return false;

    bool ok = ExportImage(img, path);
    UnloadImage(img);
    return ok;
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

static bool write_export_json(const char *id, int width, int height) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.export.json", EXPORT_DIR, id);

    cJSON *root = cJSON_CreateObject();
    cJSON *rooms = cJSON_CreateArray();
    cJSON *saves = cJSON_CreateArray();
    cJSON *teleports = cJSON_CreateArray();
    cJSON *tunnels = cJSON_CreateArray();
    if (!root || !rooms || !saves || !teleports || !tunnels) {
        cJSON_Delete(root);
        return false;
    }

    char bg_path[256];
    char col_path[256];
    snprintf(bg_path, sizeof(bg_path), "%s/%s-background.png", EXPORT_DIR, id);
    snprintf(col_path, sizeof(col_path), "%s/%s.png", EXPORT_DIR, id);

    cJSON_AddStringToObject(root, "id", id);
    cJSON_AddNumberToObject(root, "tile_size", TILE_SIZE);
    cJSON_AddNumberToObject(root, "width", width);
    cJSON_AddNumberToObject(root, "height", height);
    cJSON_AddStringToObject(root, "background_png", bg_path);
    cJSON_AddStringToObject(root, "collision_png", col_path);

    cJSON *room = cJSON_CreateObject();
    cJSON_AddStringToObject(room, "id", "r-1");
    cJSON_AddStringToObject(room, "name", "Start");
    cJSON_AddBoolToObject(room, "isolated", false);
    cJSON_AddNumberToObject(room, "x", 0);
    cJSON_AddNumberToObject(room, "y", 0);
    cJSON_AddNumberToObject(room, "w", width);
    cJSON_AddNumberToObject(room, "h", height);
    cJSON_AddNumberToObject(room, "view_y", 0);
    cJSON_AddNumberToObject(room, "view_h", height);
    cJSON_AddItemToArray(rooms, room);

    cJSON *save = cJSON_CreateObject();
    cJSON_AddNumberToObject(save, "index", 0);
    cJSON_AddNumberToObject(save, "x", width / 2);
    cJSON_AddNumberToObject(save, "y", height - 20);
    cJSON_AddStringToObject(save, "room", "r-1");
    cJSON_AddItemToArray(saves, save);

    cJSON_AddItemToObject(root, "rooms", rooms);
    cJSON_AddItemToObject(root, "saves", saves);
    cJSON_AddItemToObject(root, "teleports", teleports);
    cJSON_AddItemToObject(root, "tunnels", tunnels);

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

bool act_create_empty_act(const char *id, const char *label, int width, int height) {
    if (!id || !label || width <= 0 || height <= 0 || width % TILE_SIZE != 0 || height % TILE_SIZE != 0)
        return false;

    if (!ensure_dir("resources/visual") || !ensure_dir(EXPORT_DIR))
        return false;

    char bg_path[256];
    char col_path[256];
    snprintf(bg_path, sizeof(bg_path), "%s/%s-background.png", EXPORT_DIR, id);
    snprintf(col_path, sizeof(col_path), "%s/%s.png", EXPORT_DIR, id);

    Color sky = (Color){ 135, 206, 235, 255 };
    if (!write_png_solid(bg_path, width, height, sky)) {
        TraceLog(LOG_WARNING, "act_create: failed to write %s", bg_path);
        return false;
    }

    if (!write_png_transparent(col_path, width, height)) {
        TraceLog(LOG_WARNING, "act_create: failed to write %s", col_path);
        return false;
    }

    if (!write_export_json(id, width, height))
        return false;

    if (!write_empty_gameplay_json(id, width, height))
        return false;

    if (!manifest_has_id(id) && !act_registry_append_manifest_entry(ACT_MANIFEST_PATH, id, label)) {
        TraceLog(LOG_WARNING, "act_create: failed to update manifest");
        return false;
    }

    act_create_try_aseprite_file(id, width, height);
    return true;
}

static const char *find_aseprite_cli(void) {
    if (file_exists("/Applications/Aseprite.app/Contents/MacOS/Aseprite"))
        return "/Applications/Aseprite.app/Contents/MacOS/Aseprite";
    return "aseprite";
}

bool act_create_try_aseprite_file(const char *id, int width, int height) {
    if (!id)
        return false;

    const char *aseprite = find_aseprite_cli();
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" -b -script-param act_id=%s -script-param width=%d -script-param height=%d "
             "-script-param template=%s -script \"%s\" 2>/dev/null",
             aseprite, id, width, height, TILESET_TEMPLATE, CREATE_ACT_SCRIPT);

    int rc = system(cmd);
    if (rc != 0) {
        TraceLog(LOG_WARNING, "act_create: Aseprite not available or create-act.lua failed (rc=%d)", rc);
        return false;
    }

    TraceLog(LOG_INFO, "act_create: wrote resources/visual/%s.aseprite", id);
    return true;
}
