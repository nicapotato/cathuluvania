#ifndef ACT_CREATE_H
#define ACT_CREATE_H

#include "act_registry.h"
#include <stdbool.h>

#define ACT_TEMPLATE_ASEPRITE "resources/visual/template.aseprite"

bool act_create_generate_id(char *out_id, int out_cap);
bool act_create_from_template(const char *id, const char *label);

#endif
