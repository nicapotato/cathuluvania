#ifndef LAYER_COMPOSITE_H
#define LAYER_COMPOSITE_H

#include "cute_aseprite.h"
#include "raylib.h"
#include <stdbool.h>

bool layer_composite_resolve_layer_name(const ase_t *ase, const char *wanted,
                                        char *resolved, int resolved_cap);
bool layer_composite_bake_layer_image(const ase_t *ase, int frame_index,
                                      const char *layer_name, Image *out_image);
bool layer_composite_bake_layer_texture(const ase_t *ase, int frame_index,
                                        const char *layer_name, Texture2D *out_texture);
bool layer_composite_image_has_content(const Image *image);
bool layer_composite_bake_edit_layer_from_file(const char *ase_path, int frame_index,
                                               Image *out_image);

#endif
