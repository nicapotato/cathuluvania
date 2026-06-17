#include "act_export.h"

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define WRITE_COLLISION_SCRIPT "scripts/aesprite/write-collision-layer.lua"
#define EXPORT_LEVEL_SCRIPT    "scripts/aesprite/export-act-level.lua"

static bool file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0;
}

bool act_export_find_cli(char *out_path, int out_cap) {
    if (!out_path || out_cap <= 0)
        return false;

    if (file_exists("/Applications/Aseprite.app/Contents/MacOS/Aseprite")) {
        snprintf(out_path, (size_t)out_cap, "/Applications/Aseprite.app/Contents/MacOS/Aseprite");
        return true;
    }

    snprintf(out_path, (size_t)out_cap, "aseprite");
    return true;
}

static int run_aseprite_cmd(const char *cmd) {
    TraceLog(LOG_INFO, "act_export: %s", cmd);
    return system(cmd);
}

bool act_export_write_collision_layer(const char *aseprite_path, const char *png_path) {
    if (!aseprite_path || !png_path)
        return false;

    char aseprite_cli[256];
    if (!act_export_find_cli(aseprite_cli, (int)sizeof(aseprite_cli)))
        return false;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" -b -script-param aseprite=\"%s\" -script-param png=\"%s\" "
             "-script \"%s\" 2>&1",
             aseprite_cli, aseprite_path, png_path, WRITE_COLLISION_SCRIPT);

    int rc = run_aseprite_cmd(cmd);
    if (rc != 0) {
        TraceLog(LOG_WARNING, "act_export: write-collision-layer failed (rc=%d)", rc);
        return false;
    }
    return true;
}

bool act_export_run_level_export(const char *aseprite_path) {
    if (!aseprite_path)
        return false;

    char aseprite_cli[256];
    if (!act_export_find_cli(aseprite_cli, (int)sizeof(aseprite_cli)))
        return false;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" -b \"%s\" -script \"%s\" 2>&1",
             aseprite_cli, aseprite_path, EXPORT_LEVEL_SCRIPT);

    int rc = run_aseprite_cmd(cmd);
    if (rc != 0) {
        TraceLog(LOG_WARNING, "act_export: export-act-level failed (rc=%d)", rc);
        return false;
    }
    return true;
}

bool act_export_save_collision(const char *aseprite_path, const Image *collision_image) {
    if (!aseprite_path || !collision_image || !collision_image->data)
        return false;

    const char *tmp_png = "resources/visual/layers/.editor-collision-tmp.png";
    if (!ExportImage(*collision_image, tmp_png)) {
        TraceLog(LOG_WARNING, "act_export: failed to write temp png");
        return false;
    }

    if (!act_export_write_collision_layer(aseprite_path, tmp_png))
        return false;

    if (!act_export_run_level_export(aseprite_path))
        return false;

    return true;
}
