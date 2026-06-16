#ifndef IMAGE_BLIT_H
#define IMAGE_BLIT_H

#include "raylib.h"

/* Alpha-composite blit for UNCOMPRESSED_R8G8B8A8 images (ImageDraw fallback for older raylib). */
static inline void image_blit_rgba(Image *dst, Image src, int dst_x, int dst_y) {
    if (!dst || !dst->data || !src.data)
        return;
    if (dst->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
        || src.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
        return;

    Color *src_px = (Color *)src.data;
    Color *dst_px = (Color *)dst->data;

    for (int y = 0; y < src.height; y++) {
        int dy = dst_y + y;
        if (dy < 0 || dy >= dst->height)
            continue;

        for (int x = 0; x < src.width; x++) {
            int dx = dst_x + x;
            if (dx < 0 || dx >= dst->width)
                continue;

            Color s = src_px[y * src.width + x];
            if (s.a == 0)
                continue;

            int di = dy * dst->width + dx;
            if (s.a == 255) {
                dst_px[di] = s;
                continue;
            }

            Color d = dst_px[di];
            float a = (float)s.a / 255.f;
            float ia = 1.f - a;
            dst_px[di] = (Color){
                (unsigned char)((float)s.r * a + (float)d.r * ia),
                (unsigned char)((float)s.g * a + (float)d.g * ia),
                (unsigned char)((float)s.b * a + (float)d.b * ia),
                (unsigned char)((float)s.a + (float)d.a * ia)
            };
        }
    }
}

#endif
