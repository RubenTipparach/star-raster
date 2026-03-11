#ifndef SR_PALETTE_H
#define SR_PALETTE_H

/*  Palette-indexed texture + shade-shift rendering.
 *
 *  Textures store palette indices (1 byte per pixel).
 *  At render time, a shade row is chosen based on lighting intensity,
 *  and the final color is looked up via:
 *      color = pal_colors[ pal_shift_lut[shade_row][texel_index] ]
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Generated palette data ─────────────────────────────────────── */

#include "../assets/palette_lut.h"

/* ── Indexed texture ────────────────────────────────────────────── */

typedef struct {
    uint8_t *indices;   /* palette index per pixel */
    int      width, height;
} sr_indexed_texture;

/* Load a .idx file (u16 width, u16 height, w*h bytes) */
static inline sr_indexed_texture sr_indexed_load(const char *path) {
    sr_indexed_texture tex = {0};
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Failed to load indexed texture: %s\n", path); return tex; }

    uint16_t w, h;
    fread(&w, 2, 1, f);
    fread(&h, 2, 1, f);
    tex.width  = w;
    tex.height = h;
    tex.indices = (uint8_t *)malloc(w * h);
    fread(tex.indices, 1, w * h, f);
    fclose(f);
    return tex;
}

static inline void sr_indexed_free(sr_indexed_texture *tex) {
    free(tex->indices);
    tex->indices = NULL;
}

/* Sample an indexed texture at (u,v), returning the palette index */
static inline uint8_t sr_indexed_sample(const sr_indexed_texture *tex, float u, float v) {
    u = u - (float)(int)u; if (u < 0) u += 1.0f;
    v = v - (float)(int)v; if (v < 0) v += 1.0f;
    int x = (int)(u * tex->width)  % tex->width;
    int y = (int)(v * tex->height) % tex->height;
    if (x < 0) x += tex->width;
    if (y < 0) y += tex->height;
    return tex->indices[y * tex->width + x];
}

/* ── Shade row computation ──────────────────────────────────────── */

/*  Convert a lighting intensity (0.0 = full dark, 1.0 = default, 2.0 = full bright)
 *  to a shade row index.
 *  intensity < 1.0 → darker rows (0..PAL_MID_ROW-1)
 *  intensity = 1.0 → middle row (PAL_MID_ROW)
 *  intensity > 1.0 → brighter rows (PAL_MID_ROW+1..PAL_SHADES-1)
 */
static inline int sr_shade_row(float intensity) {
    /* Map: 0.0 → row 0, 1.0 → PAL_MID_ROW, 2.0 → PAL_SHADES-1 */
    int row;
    if (intensity <= 1.0f) {
        row = (int)(intensity * PAL_MID_ROW);
    } else {
        float bright_t = (intensity - 1.0f); /* 0..1 maps to mid+1..max */
        row = PAL_MID_ROW + (int)(bright_t * (PAL_SHADES - 1 - PAL_MID_ROW));
    }
    if (row < 0) row = 0;
    if (row >= PAL_SHADES) row = PAL_SHADES - 1;
    return row;
}

/* Dithered shade row: uses Bayer 4x4 to blend between adjacent rows,
 * eliminating banding artifacts on smooth gradients. */
static inline int sr_shade_row_dithered(float intensity, int px, int py) {
    static const uint8_t bayer4[4][4] = {
        {  0,  8,  2, 10 },
        { 12,  4, 14,  6 },
        {  3, 11,  1,  9 },
        { 15,  7, 13,  5 }
    };
    /* Compute continuous row as float */
    float row_f;
    if (intensity <= 1.0f) {
        row_f = intensity * PAL_MID_ROW;
    } else {
        float bright_t = (intensity - 1.0f);
        row_f = PAL_MID_ROW + bright_t * (PAL_SHADES - 1 - PAL_MID_ROW);
    }
    if (row_f < 0.0f) row_f = 0.0f;
    if (row_f > (float)(PAL_SHADES - 1)) row_f = (float)(PAL_SHADES - 1);

    int row_lo = (int)row_f;
    float frac = row_f - (float)row_lo;
    if (row_lo >= PAL_SHADES - 1) return PAL_SHADES - 1;

    float threshold = (float)bayer4[py & 3][px & 3] * (1.0f / 16.0f);
    return (frac > threshold) ? row_lo + 1 : row_lo;
}

/* Look up final color: shade_row + palette_index → ABGR color */
static inline uint32_t sr_palette_lookup(int shade_row, uint8_t pal_idx) {
    if (pal_idx >= PAL_COLORS) pal_idx = 0;
    uint8_t shifted_idx = pal_shift_lut[shade_row][pal_idx];
    return pal_colors[shifted_idx];
}

#endif /* SR_PALETTE_H */
