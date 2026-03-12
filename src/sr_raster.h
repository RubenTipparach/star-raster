#ifndef SR_RASTER_H
#define SR_RASTER_H

#include <stdint.h>
#include <stdbool.h>
#include "sr_math.h"
#include "sr_texture.h"

/* ── Framebuffer ─────────────────────────────────────────────────── */

typedef struct {
    uint32_t *color;
    float    *depth;
    int       width, height;
} sr_framebuffer;

sr_framebuffer sr_framebuffer_create(int w, int h);
void           sr_framebuffer_destroy(sr_framebuffer *fb);
void           sr_framebuffer_clear(sr_framebuffer *fb, uint32_t color, float depth);

/* ── Fog (Bayer-dithered distance fog) ───────────────────────────── */

void sr_fog_set(uint32_t color, float near_dist, float far_dist);
void sr_fog_disable(void);

/* ── Stats ───────────────────────────────────────────────────────── */

void sr_stats_reset(void);
int  sr_stats_tri_count(void);

/* ── Vertex ──────────────────────────────────────────────────────── */

typedef struct {
    sr_vec3  pos;
    sr_vec2  uv;
    uint32_t color;   /* vertex color, multiplied with texture (Gouraud) */
    float    wx, wy, wz;     /* world position (for pixel lighting) */
    float    wnx, wny, wnz;  /* world normal (for pixel lighting) */
} sr_vertex;

/* White vertex (unlit / full bright) */
static inline sr_vertex sr_vert(float x, float y, float z, float u, float v) {
    return (sr_vertex){ {x,y,z}, {u,v}, 0xFFFFFFFF, 0,0,0, 0,0,0 };
}

/* Colored / pre-lit vertex */
static inline sr_vertex sr_vert_c(float x, float y, float z,
                                  float u, float v, uint32_t color) {
    return (sr_vertex){ {x,y,z}, {u,v}, color, 0,0,0, 0,0,0 };
}

/* Vertex with world position and normal (for pixel lighting) */
static inline sr_vertex sr_vert_world(float x, float y, float z,
                                      float u, float v, uint32_t color,
                                      float wx, float wy, float wz,
                                      float wnx, float wny, float wnz) {
    return (sr_vertex){ {x,y,z}, {u,v}, color, wx,wy,wz, wnx,wny,wnz };
}

/* ── Drawing ─────────────────────────────────────────────────────── */
/* Vertex colors are interpolated (Gouraud) and multiplied with texture. */

void sr_draw_triangle(sr_framebuffer *fb,
                      sr_vertex v0, sr_vertex v1, sr_vertex v2,
                      const sr_texture *tex,
                      const sr_mat4 *mvp);

void sr_draw_quad(sr_framebuffer *fb,
                  sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                  const sr_texture *tex,
                  const sr_mat4 *mvp);

void sr_draw_triangle_doublesided(sr_framebuffer *fb,
                                  sr_vertex v0, sr_vertex v1, sr_vertex v2,
                                  const sr_texture *tex,
                                  const sr_mat4 *mvp);

void sr_draw_quad_doublesided(sr_framebuffer *fb,
                              sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                              const sr_texture *tex,
                              const sr_mat4 *mvp);

/* ── Palette-indexed drawing ──────────────────────────────────────
 * Vertex color R channel encodes lighting intensity (0-255).
 * 128 = default shade (mid row), 0 = darkest, 255 = brightest.
 * The indexed texture is sampled for a palette index, then the
 * shade-shifted color is looked up from the palette LUT.
 */

#include "sr_palette.h"

void sr_draw_triangle_indexed(sr_framebuffer *fb,
                               sr_vertex v0, sr_vertex v1, sr_vertex v2,
                               const sr_indexed_texture *tex,
                               const sr_mat4 *mvp);

void sr_draw_quad_indexed(sr_framebuffer *fb,
                           sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                           const sr_indexed_texture *tex,
                           const sr_mat4 *mvp);

void sr_draw_quad_indexed_doublesided(sr_framebuffer *fb,
                                       sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                                       const sr_indexed_texture *tex,
                                       const sr_mat4 *mvp);

/* ── Per-pixel lighting callback ─────────────────────────────────── */

/* Callback: given world pos (px,py,pz) and normal (nx,ny,nz), return intensity */
typedef float (*sr_pixel_light_fn)(float px, float py, float pz,
                                   float nx, float ny, float nz);

void sr_set_pixel_light_fn(sr_pixel_light_fn fn);

void sr_draw_triangle_indexed_pixellit(sr_framebuffer *fb,
                                        sr_vertex v0, sr_vertex v1, sr_vertex v2,
                                        const sr_indexed_texture *tex,
                                        const sr_mat4 *mvp);

void sr_draw_quad_indexed_pixellit(sr_framebuffer *fb,
                                    sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                                    const sr_indexed_texture *tex,
                                    const sr_mat4 *mvp);

void sr_draw_quad_indexed_doublesided_pixellit(sr_framebuffer *fb,
                                                sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                                                const sr_indexed_texture *tex,
                                                const sr_mat4 *mvp);

/* ── Depth-only rendering (for shadow maps) ──────────────────────── */

void sr_draw_triangle_depth_only(sr_framebuffer *fb,
                                  sr_vertex v0, sr_vertex v1, sr_vertex v2,
                                  const sr_mat4 *mvp);

void sr_draw_quad_depth_only(sr_framebuffer *fb,
                              sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                              const sr_mat4 *mvp);

#endif /* SR_RASTER_H */
