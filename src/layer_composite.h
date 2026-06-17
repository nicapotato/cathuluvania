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
bool layer_composite_bake_frame_excluding_layer(const ase_t *ase, int frame_index,
                                                const char *exclude_layer, Image *out_image);
bool layer_composite_bake_frame_excluding_layers(const ase_t *ase, int frame_index,
                                                 const char *const *exclude_layers,
                                                 int exclude_layer_count, Image *out_image);

#endif
