#ifndef SR_TEXTURE_H
#define SR_TEXTURE_H

#include <stdint.h>
#include <math.h>

typedef struct {
    uint32_t *pixels;   /* RGBA8 packed as 0xAABBGGRR */
    int width, height;
} sr_texture;

/* Load a PNG/JPG/BMP texture. Returns {0} on failure. */
sr_texture sr_texture_load(const char *path);

/* Free texture memory. */
void sr_texture_free(sr_texture *tex);

/* Sample texture at (u,v) with nearest-neighbor filtering. u,v in [0,1]. */
static inline uint32_t sr_texture_sample(const sr_texture *tex, float u, float v) {
    /* Wrap UVs */
    u = u - floorf(u);
    v = v - floorf(v);
    int x = (int)(u * tex->width)  % tex->width;
    int y = (int)(v * tex->height) % tex->height;
    if (x < 0) x += tex->width;
    if (y < 0) y += tex->height;
    return tex->pixels[y * tex->width + x];
}

#endif /* SR_TEXTURE_H */
