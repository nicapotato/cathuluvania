#ifndef ACT_CREATE_H
#define ACT_CREATE_H

#include "act_registry.h"
#include <stdbool.h>

#define ACT_CREATE_DEFAULT_WIDTH  320
#define ACT_CREATE_DEFAULT_HEIGHT 240

bool act_create_generate_id(char *out_id, int out_cap);
bool act_create_empty_act(const char *id, const char *label, int width, int height);
bool act_create_try_aseprite_file(const char *id, int width, int height);

#endif
