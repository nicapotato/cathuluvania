#include "player_sprite.h"
#include "main.h"
#include "layer_composite.h"
#include "image_blit.h"
#include "cute_aseprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *const PLAYER_SPRITE_HIDDEN_LAYERS[] = {
    PLAYER_SPRITE_COLLISION_LAYER,
    PLAYER_SPRITE_BACKGROUND_LAYER,
};

#define PLAYER_SPRITE_HIDDEN_LAYER_COUNT \
    ((int)(sizeof(PLAYER_SPRITE_HIDDEN_LAYERS) / sizeof(PLAYER_SPRITE_HIDDEN_LAYERS[0])))

static bool player_sprite_attach_texture(ase_t *ase, Image *strip) {
    if (!ase || !strip || !strip->data)
        return false;

    int transparency = ase->transparent_palette_entry_index;
    if (transparency >= 0 && transparency < ase->palette.entry_count) {
        ase_color_t transparent_color = ase->palette.entries[transparency].color;
        Color key = {
            .r = transparent_color.r,
            .g = transparent_color.g,
            .b = transparent_color.b,
            .a = transparent_color.a
        };
        ImageColorReplace(strip, key, BLANK);
    }

    Texture2D texture = LoadTextureFromImage(*strip);
    if (texture.id == 0)
        return false;

    SetTextureFilter(texture, TEXTURE_FILTER_POINT);

    if (ase->mem_ctx)
        MemFree(ase->mem_ctx);

    Texture2D *stored = (Texture2D *)MemAlloc(sizeof(Texture2D));
    if (!stored) {
        UnloadTexture(texture);
        return false;
    }
    *stored = texture;
    ase->mem_ctx = stored;
    return true;
}

bool player_frame_anchor_from_image(const Image *image, PlayerFrameAnchor *out) {
    if (!image || !out || !image->data || image->width <= 0 || image->height <= 0)
        return false;

    int min_x = image->width;
    int min_y = image->height;
    int max_x = -1;
    int max_y = -1;

    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            if (GetImageColor(*image, x, y).a == 0)
                continue;
            if (x < min_x) min_x = x;
            if (y < min_y) min_y = y;
            if (x > max_x) max_x = x;
            if (y > max_y) max_y = y;
        }
    }

    if (max_x < 0)
        return false;

    out->bbox_l = (float)min_x;
    out->bbox_t = (float)min_y;
    out->bbox_r = (float)(max_x + 1);
    out->bbox_b = (float)(max_y + 1);
    out->center_x = (out->bbox_l + out->bbox_r) * 0.5f;
    out->bottom_y = out->bbox_b;
    out->valid = true;
    return true;
}

static void player_sprite_default_anchor(const ase_t *ase, PlayerFrameAnchor *out) {
    out->center_x = (float)ase->w * 0.5f;
    out->bottom_y = (float)ase->h * 0.75f;
    out->bbox_l = out->center_x - 6.0f;
    out->bbox_t = out->bottom_y - 12.0f;
    out->bbox_r = out->center_x + 6.0f;
    out->bbox_b = out->bottom_y;
    out->valid = true;
}

static bool player_sprite_build_anchors(PlayerSprite *sprite, ase_t *ase) {
    sprite->frame_count = ase->frame_count;
    sprite->frame_anchors = (PlayerFrameAnchor *)calloc((size_t)ase->frame_count,
                                                        sizeof(PlayerFrameAnchor));
    if (!sprite->frame_anchors)
        return false;

    player_sprite_default_anchor(ase, &sprite->fallback_anchor);

    bool collision_layer_found = false;
    char resolved[64];
    if (layer_composite_resolve_layer_name(ase, PLAYER_SPRITE_COLLISION_LAYER, resolved,
                                           (int)sizeof(resolved)))
        collision_layer_found = true;

    if (!collision_layer_found) {
        TraceLog(LOG_WARNING,
                 "PLAYER_SPRITE: no \"%s\" layer — using default draw anchors",
                 PLAYER_SPRITE_COLLISION_LAYER);
        for (int i = 0; i < ase->frame_count; i++)
            sprite->frame_anchors[i] = sprite->fallback_anchor;
        return true;
    }

    for (int i = 0; i < ase->frame_count; i++) {
        Image collision = { 0 };
        if (!layer_composite_bake_layer_image(ase, i, PLAYER_SPRITE_COLLISION_LAYER, &collision)) {
            sprite->frame_anchors[i] = sprite->fallback_anchor;
            continue;
        }

        if (!player_frame_anchor_from_image(&collision, &sprite->frame_anchors[i])) {
            TraceLog(LOG_WARNING, "PLAYER_SPRITE: frame %d collision layer empty — using fallback", i);
            sprite->frame_anchors[i] = sprite->fallback_anchor;
        }
        UnloadImage(collision);
    }

    if (sprite->frame_anchors[0].valid)
        sprite->fallback_anchor = sprite->frame_anchors[0];

    return true;
}

bool player_sprite_load(PlayerSprite *sprite, const char *path) {
    if (!sprite)
        return false;

    memset(sprite, 0, sizeof(*sprite));

    if (!IsWindowReady()) {
        TraceLog(LOG_ERROR, "PLAYER_SPRITE: window must be ready before loading");
        return false;
    }

    int bytes_read = 0;
    unsigned char *file_data = LoadFileData(path, &bytes_read);
    if (bytes_read <= 0 || !file_data) {
        TraceLog(LOG_ERROR, "PLAYER_SPRITE: failed to read \"%s\"", path);
        return false;
    }

    ase_t *ase = cute_aseprite_load_from_memory(file_data, bytes_read, NULL);
    UnloadFileData(file_data);

    if (!ase || ase->frame_count <= 0 || ase->w <= 0 || ase->h <= 0) {
        TraceLog(LOG_ERROR, "PLAYER_SPRITE: failed to parse \"%s\"", path);
        if (ase)
            cute_aseprite_free(ase);
        return false;
    }

    Image strip = GenImageColor(ase->w * ase->frame_count, ase->h, BLANK);
    if (!strip.data) {
        cute_aseprite_free(ase);
        return false;
    }

    for (int i = 0; i < ase->frame_count; i++) {
        Image frame_image = { 0 };
        if (!layer_composite_bake_frame_excluding_layers(ase, i, PLAYER_SPRITE_HIDDEN_LAYERS,
                                                         PLAYER_SPRITE_HIDDEN_LAYER_COUNT,
                                                         &frame_image)) {
            UnloadImage(strip);
            cute_aseprite_free(ase);
            return false;
        }
        image_blit_rgba(&strip, frame_image, i * ase->w, 0);
        UnloadImage(frame_image);
    }

    if (!player_sprite_attach_texture(ase, &strip)) {
        UnloadImage(strip);
        cute_aseprite_free(ase);
        return false;
    }
    UnloadImage(strip);

    if (!player_sprite_build_anchors(sprite, ase)) {
        if (ase->mem_ctx) {
            UnloadTexture(*(Texture2D *)ase->mem_ctx);
            MemFree(ase->mem_ctx);
            ase->mem_ctx = NULL;
        }
        cute_aseprite_free(ase);
        return false;
    }

    sprite->capsule_params.valid = false;
    {
        const PlayerFrameAnchor *anchor = player_sprite_frame_anchor(sprite, 0);
        if (anchor && anchor->valid) {
            float w = (anchor->bbox_r - anchor->bbox_l) - 2.0f * PLAYER_COLLISION_SHAVE_X;
            float h = anchor->bbox_b - anchor->bbox_t;
            if (w > 0.0f && h > 0.0f) {
                sprite->capsule_params.half_w = w * 0.5f;
                sprite->capsule_params.height = PLAYER_CAPSULE_HEIGHT;
                sprite->capsule_params.corner_r = PLAYER_CAPSULE_CORNER_R;
                if (sprite->capsule_params.corner_r > sprite->capsule_params.half_w)
                    sprite->capsule_params.corner_r = sprite->capsule_params.half_w;
                sprite->capsule_params.valid = true;
            }
        }
    }

    sprite->aseprite.ase = ase;
    sprite->loaded = true;
    TraceLog(LOG_INFO, "PLAYER_SPRITE: loaded \"%s\" (%dx%d, %d frames)",
             path, ase->w, ase->h, ase->frame_count);
    return true;
}

void player_sprite_unload(PlayerSprite *sprite) {
    if (!sprite)
        return;

    if (IsAsepriteValid(sprite->aseprite))
        UnloadAseprite(sprite->aseprite);

    free(sprite->frame_anchors);
    memset(sprite, 0, sizeof(*sprite));
}

const PlayerFrameAnchor *player_sprite_frame_anchor(const PlayerSprite *sprite, int frame) {
    if (!sprite || !sprite->loaded || !sprite->frame_anchors)
        return &sprite->fallback_anchor;
    if (frame < 0 || frame >= sprite->frame_count)
        return &sprite->fallback_anchor;

    const PlayerFrameAnchor *anchor = &sprite->frame_anchors[frame];
    if (anchor->valid)
        return anchor;
    return &sprite->fallback_anchor;
}

bool player_sprite_world_collision_aabb(const PlayerSprite *sprite, int frame, bool flip_h,
                                        float world_feet_x, float world_feet_y,
                                        float *out_l, float *out_t, float *out_r, float *out_b) {
    if (!out_l || !out_t || !out_r || !out_b)
        return false;

    const PlayerFrameAnchor *anchor = player_sprite_frame_anchor(sprite, frame);
    if (!sprite || !sprite->loaded || !anchor || !anchor->valid)
        return false;

    float scale = PLAYER_SPRITE_DRAW_SCALE;
    float bl = anchor->bbox_l;
    float br = anchor->bbox_r;
    if (flip_h) {
        float cx = anchor->center_x;
        bl = 2.0f * cx - anchor->bbox_r;
        br = 2.0f * cx - anchor->bbox_l;
    }

    *out_l = world_feet_x + (bl - anchor->center_x) * scale;
    *out_r = world_feet_x + (br - anchor->center_x) * scale;
    *out_t = world_feet_y + (anchor->bbox_t - anchor->bottom_y) * scale;
    *out_b = world_feet_y + (anchor->bbox_b - anchor->bottom_y) * scale;

    *out_l += PLAYER_COLLISION_SHAVE_X;
    *out_r -= PLAYER_COLLISION_SHAVE_X;
    if (*out_r <= *out_l)
        *out_r = *out_l + 1.0f;

    return true;
}

bool player_sprite_world_collision_oval(const PlayerSprite *sprite, int frame, bool flip_h,
                                        float world_feet_x, float world_feet_y,
                                        PlayerCollisionOval *out) {
    if (!out)
        return false;

    out->valid = false;

    float l, t, r, b;
    if (!player_sprite_world_collision_aabb(sprite, frame, flip_h, world_feet_x, world_feet_y,
                                            &l, &t, &r, &b))
        return false;

    out->cx = (l + r) * 0.5f;
    out->cy = (t + b) * 0.5f;
    out->rx = (r - l) * 0.5f * PLAYER_COLLISION_OVAL_RX_SCALE;
    out->ry = (b - t) * 0.5f * PLAYER_COLLISION_OVAL_RY_SCALE;
    if (out->rx < 1.0f)
        out->rx = 1.0f;
    if (out->ry < 1.0f)
        out->ry = 1.0f;
    out->valid = true;
    return true;
}

void player_sprite_draw_tag(const PlayerSprite *sprite, AsepriteTag tag, Vector2 world_feet,
                            bool flip_h, Color tint) {
    if (!sprite || !sprite->loaded || !IsAsepriteTagValid(tag))
        return;

    ase_t *ase = sprite->aseprite.ase;
    if (!ase)
        return;

    const PlayerFrameAnchor *anchor = player_sprite_frame_anchor(sprite, tag.currentFrame);
    float scale = PLAYER_SPRITE_DRAW_SCALE;
    float fw = (float)ase->w * scale;
    float fh = (float)ase->h * scale;

    /* DrawTexturePro origin is in destination (scaled) space, not source pixels. */
    float origin_x = anchor->center_x * scale;
    if (flip_h)
        origin_x = fw - origin_x;

    Vector2 origin = { origin_x, anchor->bottom_y * scale };
    /* Snap anchor to whole pixels so subpixel physics does not blur the sprite. */
    Rectangle dest = {
        floorf(world_feet.x + 0.5f),
        floorf(world_feet.y + 0.5f),
        fw,
        fh
    };
    DrawAsepriteTagProFlipped(tag, dest, origin, 0.0f, flip_h, false, tint);
}
