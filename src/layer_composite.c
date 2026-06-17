#include "raylib.h"
#include "cute_aseprite.h"
#include "layer_composite.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int lc_min(int a, int b) { return a < b ? a : b; }
static int lc_max(int a, int b) { return a < b ? b : a; }

static int lc_mul_un8(int a, int b) {
    int t = (a * b) + 0x80;
    return (((t >> 8) + t) >> 8);
}

static ase_color_t lc_blend(ase_color_t src, ase_color_t dst, uint8_t opacity) {
    src.a = (uint8_t)lc_mul_un8(src.a, opacity);
    int a = src.a + dst.a - lc_mul_un8(src.a, dst.a);
    int r, g, b;
    if (a == 0) {
        r = g = b = 0;
    } else {
        r = dst.r + (src.r - dst.r) * src.a / a;
        g = dst.g + (src.g - dst.g) * src.a / a;
        b = dst.b + (src.b - dst.b) * src.a / a;
    }
    return (ase_color_t){ (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a };
}

static ase_color_t lc_sample_cel(ase_t *ase, void *src, int index) {
    if (ase->mode == ASE_MODE_RGBA)
        return ((ase_color_t *)src)[index];
    if (ase->mode == ASE_MODE_GRAYSCALE) {
        uint8_t v = ((uint8_t *)src)[index * 2];
        uint8_t a = ((uint8_t *)src)[index * 2 + 1];
        return (ase_color_t){ v, v, v, a };
    }
    uint8_t palette_index = ((uint8_t *)src)[index];
    if (palette_index == ase->transparent_palette_entry_index)
        return (ase_color_t){ 0, 0, 0, 0 };
    return ase->palette.entries[palette_index].color;
}

static void lc_normalize_name(const char *in, char *out, int out_cap) {
    int j = 0;
    int i = 0;
    if (!in || !out || out_cap <= 0)
        return;

    while (in[i] && isspace((unsigned char)in[i]))
        i++;
    while (in[i] && j < out_cap - 1) {
        if (!isspace((unsigned char)in[i]))
            out[j++] = (char)tolower((unsigned char)in[i]);
        i++;
    }
    out[j] = '\0';
}

static bool lc_names_equivalent(const char *layer_name, const char *wanted) {
    char a[64];
    char b[64];
    lc_normalize_name(layer_name, a, (int)sizeof(a));
    lc_normalize_name(wanted, b, (int)sizeof(b));
    if (a[0] == '\0' || b[0] == '\0')
        return false;
    if (strcmp(a, b) == 0)
        return true;

    if (strcmp(b, "parallax") == 0) {
        if (strncmp(a, "parralax", 8) == 0 || strncmp(a, "parallax", 8) == 0)
            return true;
    }
    if (strcmp(b, "base") == 0) {
        if (strcmp(a, "earth-tileset") == 0 || strcmp(a, "earth_tileset") == 0)
            return true;
    }
    return false;
}

bool layer_composite_image_has_content(const Image *image) {
    if (!image || !image->data || image->width <= 0 || image->height <= 0)
        return false;
    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            if (GetImageColor(*image, x, y).a > 0)
                return true;
        }
    }
    return false;
}

bool layer_composite_resolve_layer_name(const ase_t *ase, const char *wanted,
                                        char *resolved, int resolved_cap) {
    if (!ase || !wanted || !resolved || resolved_cap <= 0)
        return false;

    for (int i = 0; i < ase->layer_count; i++) {
        const ase_layer_t *layer = ase->layers + i;
        if (!layer->name)
            continue;
        if (lc_names_equivalent(layer->name, wanted)) {
            snprintf(resolved, (size_t)resolved_cap, "%s", layer->name);
            return true;
        }
    }
    return false;
}

static ase_cel_t *lc_resolve_linked_cel(ase_t *ase, ase_cel_t *cel) {
    while (cel->is_linked) {
        ase_frame_t *link_frame = ase->frames + cel->linked_frame_index;
        int found = 0;
        for (int k = 0; k < link_frame->cel_count; k++) {
            if (link_frame->cels[k].layer == cel->layer) {
                cel = link_frame->cels + k;
                found = 1;
                break;
            }
        }
        if (!found)
            return NULL;
    }
    return cel;
}

static void lc_composite_frame(ase_t *ase, int frame_index, const char *layer_name,
                               ase_color_t *dst) {
    ase_frame_t *frame = ase->frames + frame_index;
    int aw = ase->w;
    int ah = ase->h;
    memset(dst, 0, (size_t)aw * (size_t)ah * sizeof(ase_color_t));

    for (int j = 0; j < frame->cel_count; j++) {
        ase_cel_t *cel = frame->cels + j;
        if (!cel->layer || !cel->layer->name)
            continue;
        if (!lc_names_equivalent(cel->layer->name, layer_name))
            continue;

        cel = lc_resolve_linked_cel(ase, cel);
        if (!cel || !cel->pixels)
            continue;

        uint8_t opacity = (uint8_t)(cel->opacity * cel->layer->opacity * 255.0f);
        int cx = cel->x;
        int cy = cel->y;
        int cw = cel->w;
        int ch = cel->h;
        int cl = -lc_min(cx, 0);
        int ct = -lc_min(cy, 0);
        int dl = lc_max(cx, 0);
        int dt = lc_max(cy, 0);
        int dr = lc_min(aw, cw + cx);
        int db = lc_min(ah, ch + cy);

        for (int dx = dl, sx = cl; dx < dr; dx++, sx++) {
            for (int dy = dt, sy = ct; dy < db; dy++, sy++) {
                int dst_index = aw * dy + dx;
                ase_color_t src_color = lc_sample_cel(ase, cel->pixels, cw * sy + sx);
                dst[dst_index] = lc_blend(src_color, dst[dst_index], opacity);
            }
        }
    }
}

static bool lc_is_excluded_layer(const char *layer_name, const char *const *exclude_layers,
                                 int exclude_layer_count) {
    if (!layer_name || !exclude_layers || exclude_layer_count <= 0)
        return false;
    for (int i = 0; i < exclude_layer_count; i++) {
        if (exclude_layers[i] && lc_names_equivalent(layer_name, exclude_layers[i]))
            return true;
    }
    return false;
}

static void lc_composite_frame_excluding_layers(ase_t *ase, int frame_index,
                                                const char *const *exclude_layers,
                                                int exclude_layer_count, ase_color_t *dst) {
    ase_frame_t *frame = ase->frames + frame_index;
    int aw = ase->w;
    int ah = ase->h;
    memset(dst, 0, (size_t)aw * (size_t)ah * sizeof(ase_color_t));

    for (int j = 0; j < frame->cel_count; j++) {
        ase_cel_t *cel = frame->cels + j;
        if (!cel->layer || !cel->layer->name)
            continue;
        if (lc_is_excluded_layer(cel->layer->name, exclude_layers, exclude_layer_count))
            continue;

        cel = lc_resolve_linked_cel(ase, cel);
        if (!cel || !cel->pixels)
            continue;

        uint8_t opacity = (uint8_t)(cel->opacity * cel->layer->opacity * 255.0f);
        int cx = cel->x;
        int cy = cel->y;
        int cw = cel->w;
        int ch = cel->h;
        int cl = -lc_min(cx, 0);
        int ct = -lc_min(cy, 0);
        int dl = lc_max(cx, 0);
        int dt = lc_max(cy, 0);
        int dr = lc_min(aw, cw + cx);
        int db = lc_min(ah, ch + cy);

        for (int dx = dl, sx = cl; dx < dr; dx++, sx++) {
            for (int dy = dt, sy = ct; dy < db; dy++, sy++) {
                int dst_index = aw * dy + dx;
                ase_color_t src_color = lc_sample_cel(ase, cel->pixels, cw * sy + sx);
                dst[dst_index] = lc_blend(src_color, dst[dst_index], opacity);
            }
        }
    }
}

static void lc_apply_transparency(ase_t *ase, Image *image) {
    int transparency = ase->transparent_palette_entry_index;
    if (transparency < 0 || transparency >= ase->palette.entry_count)
        return;
    ase_color_t transparent = ase->palette.entries[transparency].color;
    Color key = {
        .r = transparent.r,
        .g = transparent.g,
        .b = transparent.b,
        .a = transparent.a
    };
    ImageColorReplace(image, key, BLANK);
}

static bool lc_bake_pixels(ase_t *ase, int frame_index, const char *layer_name,
                           ase_color_t *pixels) {
    char resolved[64];
    if (!layer_composite_resolve_layer_name(ase, layer_name, resolved, (int)sizeof(resolved))) {
        fprintf(stderr, "layer_composite: layer \"%s\" not found\n", layer_name);
        return false;
    }
    lc_composite_frame(ase, frame_index, resolved, pixels);
    return true;
}

bool layer_composite_bake_layer_image(const ase_t *ase, int frame_index,
                                      const char *layer_name, Image *out_image) {
    if (!ase || !layer_name || !out_image)
        return false;
    if (frame_index < 0 || frame_index >= ase->frame_count)
        return false;
    if (ase->w <= 0 || ase->h <= 0)
        return false;

    ase_color_t *scratch = (ase_color_t *)MemAlloc(
        (unsigned int)(ase->w * ase->h * (int)sizeof(ase_color_t)));
    if (!scratch)
        return false;

    if (!lc_bake_pixels((ase_t *)ase, frame_index, layer_name, scratch)) {
        MemFree(scratch);
        return false;
    }

    Image image = {
        .data = scratch,
        .width = ase->w,
        .height = ase->h,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    *out_image = ImageCopy(image);
    MemFree(scratch);

    if (out_image->data == NULL)
        return false;

    lc_apply_transparency((ase_t *)ase, out_image);
    return true;
}

bool layer_composite_bake_layer_texture(const ase_t *ase, int frame_index,
                                        const char *layer_name, Texture2D *out_texture) {
    if (!out_texture)
        return false;

    Image image = { 0 };
    if (!layer_composite_bake_layer_image(ase, frame_index, layer_name, &image))
        return false;

    *out_texture = LoadTextureFromImage(image);
    UnloadImage(image);

    if (out_texture->id == 0)
        return false;

    SetTextureFilter(*out_texture, TEXTURE_FILTER_POINT);
    return true;
}

bool layer_composite_bake_frame_excluding_layers(const ase_t *ase, int frame_index,
                                                 const char *const *exclude_layers,
                                                 int exclude_layer_count, Image *out_image) {
    if (!ase || !out_image)
        return false;
    if (frame_index < 0 || frame_index >= ase->frame_count)
        return false;
    if (ase->w <= 0 || ase->h <= 0)
        return false;

    ase_color_t *scratch = (ase_color_t *)MemAlloc(
        (unsigned int)(ase->w * ase->h * (int)sizeof(ase_color_t)));
    if (!scratch)
        return false;

    lc_composite_frame_excluding_layers((ase_t *)ase, frame_index, exclude_layers,
                                        exclude_layer_count, scratch);

    Image image = {
        .data = scratch,
        .width = ase->w,
        .height = ase->h,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    *out_image = ImageCopy(image);
    MemFree(scratch);

    if (out_image->data == NULL)
        return false;

    lc_apply_transparency((ase_t *)ase, out_image);
    return true;
}

bool layer_composite_bake_frame_excluding_layer(const ase_t *ase, int frame_index,
                                                const char *exclude_layer, Image *out_image) {
    return layer_composite_bake_frame_excluding_layers(ase, frame_index, &exclude_layer,
                                                       exclude_layer ? 1 : 0, out_image);
}
