#ifndef ACT_EXPORT_H
#define ACT_EXPORT_H

#include "raylib.h"
#include <stdbool.h>

bool act_export_find_cli(char *out_path, int out_cap);
bool act_export_write_collision_layer(const char *aseprite_path, const char *png_path);
bool act_export_run_level_export(const char *aseprite_path);
bool act_export_save_collision(const char *aseprite_path, const Image *collision_image);

#endif
