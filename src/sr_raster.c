#include "sr_raster.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── Fog state ───────────────────────────────────────────────────── */

static struct {
    uint32_t color;
    float    near_dist;
    float    far_dist;
    bool     enabled;
} sr_fog_cfg;

void sr_fog_set(uint32_t color, float near_dist, float far_dist) {
    sr_fog_cfg.color     = color;
    sr_fog_cfg.near_dist = near_dist;
    sr_fog_cfg.far_dist  = far_dist;
    sr_fog_cfg.enabled   = true;
}

void sr_fog_disable(void) {
    sr_fog_cfg.enabled = false;
}

/* 4x4 Bayer dither matrix (values 0..15, normalized to 0..1 at use) */
static const uint8_t bayer4x4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 },
};

/* ── Stats ───────────────────────────────────────────────────────── */

static int tri_count;

void sr_stats_reset(void)    { tri_count = 0; }
int  sr_stats_tri_count(void) { return tri_count; }

/* ── Framebuffer ─────────────────────────────────────────────────── */

sr_framebuffer sr_framebuffer_create(int w, int h) {
    sr_framebuffer fb;
    fb.width  = w;
    fb.height = h;
    fb.color  = (uint32_t *)malloc(w * h * sizeof(uint32_t));
    fb.depth  = (float *)malloc(w * h * sizeof(float));
    return fb;
}

void sr_framebuffer_destroy(sr_framebuffer *fb) {
    free(fb->color);
    free(fb->depth);
    fb->color = NULL;
    fb->depth = NULL;
}

void sr_framebuffer_clear(sr_framebuffer *fb, uint32_t color, float depth) {
    int n = fb->width * fb->height;
    for (int i = 0; i < n; i++) {
        fb->color[i] = color;
        fb->depth[i] = depth;
    }
}

/* ── Helpers ─────────────────────────────────────────────────────── */

static inline float fminf3(float a, float b, float c) {
    float t = a < b ? a : b;
    return t < c ? t : c;
}

static inline float fmaxf3(float a, float b, float c) {
    float t = a > b ? a : b;
    return t > c ? t : c;
}

static inline int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ── Edge function ───────────────────────────────────────────────── */

static inline float edge_func(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

/* ── Clip-space vertex (position + attributes for interpolation) ── */

typedef struct {
    sr_vec4 clip;           /* clip-space position (x, y, z, w) */
    float   u, v;           /* texture coordinates */
    float   cr, cg, cb;     /* vertex color (0-255 range, for Gouraud) */
    float   wx, wy, wz;    /* world position (for pixel lighting) */
    float   wnx, wny, wnz; /* world normal (for pixel lighting) */
} clip_vertex;

/* Linearly interpolate between two clip vertices at parameter t */
static inline clip_vertex clip_lerp(clip_vertex a, clip_vertex b, float t) {
    clip_vertex r;
    r.clip.x = a.clip.x + t * (b.clip.x - a.clip.x);
    r.clip.y = a.clip.y + t * (b.clip.y - a.clip.y);
    r.clip.z = a.clip.z + t * (b.clip.z - a.clip.z);
    r.clip.w = a.clip.w + t * (b.clip.w - a.clip.w);
    r.u      = a.u      + t * (b.u      - a.u);
    r.v      = a.v      + t * (b.v      - a.v);
    r.cr     = a.cr     + t * (b.cr     - a.cr);
    r.cg     = a.cg     + t * (b.cg     - a.cg);
    r.cb     = a.cb     + t * (b.cb     - a.cb);
    r.wx     = a.wx     + t * (b.wx     - a.wx);
    r.wy     = a.wy     + t * (b.wy     - a.wy);
    r.wz     = a.wz     + t * (b.wz     - a.wz);
    r.wnx    = a.wnx    + t * (b.wnx    - a.wnx);
    r.wny    = a.wny    + t * (b.wny    - a.wny);
    r.wnz    = a.wnz    + t * (b.wnz    - a.wnz);
    return r;
}

/* ── Near-plane clipping (Sutherland-Hodgman, w >= NEAR_W) ──────── */

#define NEAR_W 0.001f

/*
 * Clip a polygon (up to 4 vertices from a clipped triangle) against
 * the near plane w >= NEAR_W. Input: in[in_count], Output: out[].
 * Returns the number of output vertices (0-4).
 */
static int clip_near_plane(const clip_vertex *in, int in_count,
                           clip_vertex *out)
{
    int out_count = 0;
    for (int i = 0; i < in_count; i++) {
        int j = (i + 1) % in_count;
        const clip_vertex *cur  = &in[i];
        const clip_vertex *next = &in[j];
        bool cur_inside  = cur->clip.w  >= NEAR_W;
        bool next_inside = next->clip.w >= NEAR_W;

        if (cur_inside) {
            out[out_count++] = *cur;
        }
        /* If edge crosses the plane, emit intersection point */
        if (cur_inside != next_inside) {
            float t = (NEAR_W - cur->clip.w) / (next->clip.w - cur->clip.w);
            out[out_count++] = clip_lerp(*cur, *next, t);
        }
    }
    return out_count;
}

/* ── Rasterize a single screen-space triangle (post-clip) ────────── */

static void rasterize_triangle(sr_framebuffer *fb,
                               clip_vertex cv0, clip_vertex cv1, clip_vertex cv2,
                               const sr_texture *tex)
{
    int W = fb->width;
    int H = fb->height;

    /* Perspective divide -> NDC */
    float inv_w0 = 1.0f / cv0.clip.w;
    float inv_w1 = 1.0f / cv1.clip.w;
    float inv_w2 = 1.0f / cv2.clip.w;

    float nx0 = cv0.clip.x * inv_w0, ny0 = cv0.clip.y * inv_w0, nz0 = cv0.clip.z * inv_w0;
    float nx1 = cv1.clip.x * inv_w1, ny1 = cv1.clip.y * inv_w1, nz1 = cv1.clip.z * inv_w1;
    float nx2 = cv2.clip.x * inv_w2, ny2 = cv2.clip.y * inv_w2, nz2 = cv2.clip.z * inv_w2;

    /* Viewport transform */
    float sx0 = (nx0 + 1.0f) * 0.5f * W, sy0 = (1.0f - ny0) * 0.5f * H;
    float sx1 = (nx1 + 1.0f) * 0.5f * W, sy1 = (1.0f - ny1) * 0.5f * H;
    float sx2 = (nx2 + 1.0f) * 0.5f * W, sy2 = (1.0f - ny2) * 0.5f * H;

    /* Backface culling */
    float area = edge_func(sx0, sy0, sx1, sy1, sx2, sy2);
    if (area <= 0.0f)
        return;

    float inv_area = 1.0f / area;

    /* Bounding box */
    int min_x = clampi((int)floorf(fminf3(sx0, sx1, sx2)), 0, W - 1);
    int max_x = clampi((int)ceilf(fmaxf3(sx0, sx1, sx2)),  0, W - 1);
    int min_y = clampi((int)floorf(fminf3(sy0, sy1, sy2)), 0, H - 1);
    int max_y = clampi((int)ceilf(fmaxf3(sy0, sy1, sy2)),  0, H - 1);

    /* Perspective-correct attribute pre-computation: UV */
    float u0_w = cv0.u * inv_w0, u1_w = cv1.u * inv_w1, u2_w = cv2.u * inv_w2;
    float v0_w = cv0.v * inv_w0, v1_w = cv1.v * inv_w1, v2_w = cv2.v * inv_w2;

    /* Perspective-correct attribute pre-computation: vertex color */
    float cr0_w = cv0.cr * inv_w0, cr1_w = cv1.cr * inv_w1, cr2_w = cv2.cr * inv_w2;
    float cg0_w = cv0.cg * inv_w0, cg1_w = cv1.cg * inv_w1, cg2_w = cv2.cg * inv_w2;
    float cb0_w = cv0.cb * inv_w0, cb1_w = cv1.cb * inv_w1, cb2_w = cv2.cb * inv_w2;

    /* Check if all vertex colors are white (skip color multiply) */
    bool all_white = (cv0.cr >= 255.0f && cv0.cg >= 255.0f && cv0.cb >= 255.0f &&
                      cv1.cr >= 255.0f && cv1.cg >= 255.0f && cv1.cb >= 255.0f &&
                      cv2.cr >= 255.0f && cv2.cg >= 255.0f && cv2.cb >= 255.0f);

    /* Fog pre-computation */
    bool do_fog = sr_fog_cfg.enabled;
    float fog_range_inv = 0.0f;
    if (do_fog && sr_fog_cfg.far_dist > sr_fog_cfg.near_dist)
        fog_range_inv = 1.0f / (sr_fog_cfg.far_dist - sr_fog_cfg.near_dist);

    /* Rasterize */
    for (int py = min_y; py <= max_y; py++) {
        for (int px = min_x; px <= max_x; px++) {
            float ppx = px + 0.5f;
            float ppy = py + 0.5f;

            /* Barycentric via edge functions */
            float w0 = edge_func(sx1, sy1, sx2, sy2, ppx, ppy);
            float w1 = edge_func(sx2, sy2, sx0, sy0, ppx, ppy);
            float w2 = edge_func(sx0, sy0, sx1, sy1, ppx, ppy);

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                continue;

            w0 *= inv_area;
            w1 *= inv_area;
            w2 *= inv_area;

            float z = nz0 * w0 + nz1 * w1 + nz2 * w2;
            int idx = py * W + px;

            if (z >= fb->depth[idx])
                continue;

            fb->depth[idx] = z;

            /* Perspective-correct interpolation */
            float iw = inv_w0 * w0 + inv_w1 * w1 + inv_w2 * w2;
            float inv_iw = 1.0f / iw;
            float u = (u0_w * w0 + u1_w * w1 + u2_w * w2) * inv_iw;
            float v = (v0_w * w0 + v1_w * w1 + v2_w * w2) * inv_iw;

            /* Sample texture */
            uint32_t color;
            if (tex && tex->pixels) {
                color = sr_texture_sample(tex, u, v);
            } else {
                color = 0xFFFFFFFF;
            }

            /* Apply interpolated vertex color (Gouraud shading) */
            if (!all_white) {
                float cr = (cr0_w * w0 + cr1_w * w1 + cr2_w * w2) * inv_iw;
                float cg = (cg0_w * w0 + cg1_w * w1 + cg2_w * w2) * inv_iw;
                float cb = (cb0_w * w0 + cb1_w * w1 + cb2_w * w2) * inv_iw;
                uint32_t tr = (uint32_t)clampf(cr, 0.0f, 255.0f);
                uint32_t tg = (uint32_t)clampf(cg, 0.0f, 255.0f);
                uint32_t tb = (uint32_t)clampf(cb, 0.0f, 255.0f);
                uint32_t r = ((color & 0xFF) * tr) >> 8;
                uint32_t g = (((color >> 8) & 0xFF) * tg) >> 8;
                uint32_t b = (((color >> 16) & 0xFF) * tb) >> 8;
                color = 0xFF000000 | (b << 16) | (g << 8) | r;
            }

            /* Bayer-dithered fog */
            if (do_fog) {
                float linear_depth = inv_iw;  /* ~ view-space distance */
                float fog_factor = (linear_depth - sr_fog_cfg.near_dist) * fog_range_inv;
                fog_factor = clampf(fog_factor, 0.0f, 1.0f);
                float threshold = (float)bayer4x4[py & 3][px & 3] * (1.0f / 16.0f);
                if (fog_factor > threshold)
                    color = sr_fog_cfg.color;
            }

            fb->color[idx] = color;
        }
    }
}

/* ── Draw triangle (with near-plane clipping) ────────────────────── */

static inline void fill_clip_vertex(clip_vertex *cv, const sr_vertex *sv, sr_mat4 mvp) {
    cv->clip = sr_mat4_mul_v4(mvp, sr_v4(sv->pos.x, sv->pos.y, sv->pos.z, 1.0f));
    cv->u  = sv->uv.x;
    cv->v  = sv->uv.y;
    cv->cr = (float)(sv->color & 0xFF);
    cv->cg = (float)((sv->color >> 8) & 0xFF);
    cv->cb = (float)((sv->color >> 16) & 0xFF);
    cv->wx  = sv->wx;  cv->wy  = sv->wy;  cv->wz  = sv->wz;
    cv->wnx = sv->wnx; cv->wny = sv->wny; cv->wnz = sv->wnz;
}

void sr_draw_triangle(sr_framebuffer *fb,
                      sr_vertex v0, sr_vertex v1, sr_vertex v2,
                      const sr_texture *tex,
                      const sr_mat4 *mvp)
{
    tri_count++;

    clip_vertex tri[3];
    fill_clip_vertex(&tri[0], &v0, *mvp);
    fill_clip_vertex(&tri[1], &v1, *mvp);
    fill_clip_vertex(&tri[2], &v2, *mvp);

    /* Quick reject: all vertices behind near plane */
    if (tri[0].clip.w < NEAR_W && tri[1].clip.w < NEAR_W && tri[2].clip.w < NEAR_W)
        return;

    /* Fast path: all vertices in front — no clipping needed */
    if (tri[0].clip.w >= NEAR_W && tri[1].clip.w >= NEAR_W && tri[2].clip.w >= NEAR_W) {
        rasterize_triangle(fb, tri[0], tri[1], tri[2], tex);
        return;
    }

    /* Near-plane clipping: produces a polygon with 3-4 vertices */
    clip_vertex clipped[4];
    int n = clip_near_plane(tri, 3, clipped);

    /* Fan-triangulate the clipped polygon */
    for (int i = 1; i + 1 < n; i++) {
        rasterize_triangle(fb, clipped[0], clipped[i], clipped[i + 1], tex);
    }
}

void sr_draw_quad(sr_framebuffer *fb,
                  sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                  const sr_texture *tex,
                  const sr_mat4 *mvp)
{
    sr_draw_triangle(fb, v0, v1, v2, tex, mvp);
    sr_draw_triangle(fb, v0, v2, v3, tex, mvp);
}

void sr_draw_triangle_doublesided(sr_framebuffer *fb,
                                  sr_vertex v0, sr_vertex v1, sr_vertex v2,
                                  const sr_texture *tex,
                                  const sr_mat4 *mvp)
{
    sr_draw_triangle(fb, v0, v1, v2, tex, mvp);
    sr_draw_triangle(fb, v0, v2, v1, tex, mvp);
}

void sr_draw_quad_doublesided(sr_framebuffer *fb,
                              sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                              const sr_texture *tex,
                              const sr_mat4 *mvp)
{
    sr_draw_quad(fb, v0, v1, v2, v3, tex, mvp);
    sr_draw_quad(fb, v0, v3, v2, v1, tex, mvp);
}

/* ── Palette-indexed rasterization ──────────────────────────────── */

/* Vertex color R channel = light intensity (0=dark, 128=default, 255=bright).
 * This is interpolated per-pixel and used to pick the shade row. */

static void rasterize_triangle_indexed(sr_framebuffer *fb,
                                        clip_vertex cv0, clip_vertex cv1, clip_vertex cv2,
                                        const sr_indexed_texture *tex)
{
    int W = fb->width;
    int H = fb->height;

    float inv_w0 = 1.0f / cv0.clip.w;
    float inv_w1 = 1.0f / cv1.clip.w;
    float inv_w2 = 1.0f / cv2.clip.w;

    float nx0 = cv0.clip.x * inv_w0, ny0 = cv0.clip.y * inv_w0, nz0 = cv0.clip.z * inv_w0;
    float nx1 = cv1.clip.x * inv_w1, ny1 = cv1.clip.y * inv_w1, nz1 = cv1.clip.z * inv_w1;
    float nx2 = cv2.clip.x * inv_w2, ny2 = cv2.clip.y * inv_w2, nz2 = cv2.clip.z * inv_w2;

    float sx0 = (nx0 + 1.0f) * 0.5f * W, sy0 = (1.0f - ny0) * 0.5f * H;
    float sx1 = (nx1 + 1.0f) * 0.5f * W, sy1 = (1.0f - ny1) * 0.5f * H;
    float sx2 = (nx2 + 1.0f) * 0.5f * W, sy2 = (1.0f - ny2) * 0.5f * H;

    float area = edge_func(sx0, sy0, sx1, sy1, sx2, sy2);
    if (area <= 0.0f) return;
    float inv_area = 1.0f / area;

    int min_x = clampi((int)floorf(fminf3(sx0, sx1, sx2)), 0, W - 1);
    int max_x = clampi((int)ceilf(fmaxf3(sx0, sx1, sx2)),  0, W - 1);
    int min_y = clampi((int)floorf(fminf3(sy0, sy1, sy2)), 0, H - 1);
    int max_y = clampi((int)ceilf(fmaxf3(sy0, sy1, sy2)),  0, H - 1);

    float u0_w = cv0.u * inv_w0, u1_w = cv1.u * inv_w1, u2_w = cv2.u * inv_w2;
    float v0_w = cv0.v * inv_w0, v1_w = cv1.v * inv_w1, v2_w = cv2.v * inv_w2;

    /* Light intensity in R channel (0-255) → will be mapped to shade row */
    float li0_w = cv0.cr * inv_w0, li1_w = cv1.cr * inv_w1, li2_w = cv2.cr * inv_w2;

    bool do_fog = sr_fog_cfg.enabled;
    float fog_range_inv = 0.0f;
    if (do_fog && sr_fog_cfg.far_dist > sr_fog_cfg.near_dist)
        fog_range_inv = 1.0f / (sr_fog_cfg.far_dist - sr_fog_cfg.near_dist);

    for (int py = min_y; py <= max_y; py++) {
        for (int px = min_x; px <= max_x; px++) {
            float ppx = px + 0.5f;
            float ppy = py + 0.5f;

            float w0 = edge_func(sx1, sy1, sx2, sy2, ppx, ppy);
            float w1 = edge_func(sx2, sy2, sx0, sy0, ppx, ppy);
            float w2 = edge_func(sx0, sy0, sx1, sy1, ppx, ppy);
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

            w0 *= inv_area; w1 *= inv_area; w2 *= inv_area;

            float z = nz0 * w0 + nz1 * w1 + nz2 * w2;
            int idx = py * W + px;
            if (z >= fb->depth[idx]) continue;
            fb->depth[idx] = z;

            float iw = inv_w0 * w0 + inv_w1 * w1 + inv_w2 * w2;
            float inv_iw = 1.0f / iw;
            float u = (u0_w * w0 + u1_w * w1 + u2_w * w2) * inv_iw;
            float v = (v0_w * w0 + v1_w * w1 + v2_w * w2) * inv_iw;

            /* Sample indexed texture */
            uint8_t pal_idx = 0;
            if (tex && tex->indices)
                pal_idx = sr_indexed_sample(tex, u, v);

            /* Interpolate light intensity and map to dithered shade row */
            float li = (li0_w * w0 + li1_w * w1 + li2_w * w2) * inv_iw;
            float intensity = clampf(li, 0.0f, 255.0f) / 128.0f; /* 0..~2 */
            int shade_row = sr_shade_row_dithered(intensity, px, py);

            uint32_t color = sr_palette_lookup(shade_row, pal_idx);

            /* Bayer-dithered fog */
            if (do_fog) {
                float linear_depth = inv_iw;
                float fog_factor = (linear_depth - sr_fog_cfg.near_dist) * fog_range_inv;
                fog_factor = clampf(fog_factor, 0.0f, 1.0f);
                float threshold = (float)bayer4x4[py & 3][px & 3] * (1.0f / 16.0f);
                if (fog_factor > threshold)
                    color = sr_fog_cfg.color;
            }

            fb->color[idx] = color;
        }
    }
}

void sr_draw_triangle_indexed(sr_framebuffer *fb,
                               sr_vertex v0, sr_vertex v1, sr_vertex v2,
                               const sr_indexed_texture *tex,
                               const sr_mat4 *mvp)
{
    tri_count++;

    clip_vertex tri[3];
    fill_clip_vertex(&tri[0], &v0, *mvp);
    fill_clip_vertex(&tri[1], &v1, *mvp);
    fill_clip_vertex(&tri[2], &v2, *mvp);

    if (tri[0].clip.w < NEAR_W && tri[1].clip.w < NEAR_W && tri[2].clip.w < NEAR_W)
        return;

    if (tri[0].clip.w >= NEAR_W && tri[1].clip.w >= NEAR_W && tri[2].clip.w >= NEAR_W) {
        rasterize_triangle_indexed(fb, tri[0], tri[1], tri[2], tex);
        return;
    }

    clip_vertex clipped[4];
    int n = clip_near_plane(tri, 3, clipped);
    for (int i = 1; i + 1 < n; i++)
        rasterize_triangle_indexed(fb, clipped[0], clipped[i], clipped[i + 1], tex);
}

void sr_draw_quad_indexed(sr_framebuffer *fb,
                           sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                           const sr_indexed_texture *tex,
                           const sr_mat4 *mvp)
{
    sr_draw_triangle_indexed(fb, v0, v1, v2, tex, mvp);
    sr_draw_triangle_indexed(fb, v0, v2, v3, tex, mvp);
}

void sr_draw_quad_indexed_doublesided(sr_framebuffer *fb,
                                       sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                                       const sr_indexed_texture *tex,
                                       const sr_mat4 *mvp)
{
    sr_draw_quad_indexed(fb, v0, v1, v2, v3, tex, mvp);
    sr_draw_quad_indexed(fb, v0, v3, v2, v1, tex, mvp);
}

/* ── Per-pixel lighting rasterization ─────────────────────────────── */

static sr_pixel_light_fn s_pixel_light_fn = NULL;

void sr_set_pixel_light_fn(sr_pixel_light_fn fn) {
    s_pixel_light_fn = fn;
}

static void rasterize_triangle_indexed_pixellit(sr_framebuffer *fb,
                                                 clip_vertex cv0, clip_vertex cv1, clip_vertex cv2,
                                                 const sr_indexed_texture *tex)
{
    int W = fb->width;
    int H = fb->height;

    float inv_w0 = 1.0f / cv0.clip.w;
    float inv_w1 = 1.0f / cv1.clip.w;
    float inv_w2 = 1.0f / cv2.clip.w;

    float nx0 = cv0.clip.x * inv_w0, ny0 = cv0.clip.y * inv_w0, nz0 = cv0.clip.z * inv_w0;
    float nx1 = cv1.clip.x * inv_w1, ny1 = cv1.clip.y * inv_w1, nz1 = cv1.clip.z * inv_w1;
    float nx2 = cv2.clip.x * inv_w2, ny2 = cv2.clip.y * inv_w2, nz2 = cv2.clip.z * inv_w2;

    float sx0 = (nx0 + 1.0f) * 0.5f * W, sy0 = (1.0f - ny0) * 0.5f * H;
    float sx1 = (nx1 + 1.0f) * 0.5f * W, sy1 = (1.0f - ny1) * 0.5f * H;
    float sx2 = (nx2 + 1.0f) * 0.5f * W, sy2 = (1.0f - ny2) * 0.5f * H;

    float area = edge_func(sx0, sy0, sx1, sy1, sx2, sy2);
    if (area <= 0.0f) return;
    float inv_area = 1.0f / area;

    int min_x = clampi((int)floorf(fminf3(sx0, sx1, sx2)), 0, W - 1);
    int max_x = clampi((int)ceilf(fmaxf3(sx0, sx1, sx2)),  0, W - 1);
    int min_y = clampi((int)floorf(fminf3(sy0, sy1, sy2)), 0, H - 1);
    int max_y = clampi((int)ceilf(fmaxf3(sy0, sy1, sy2)),  0, H - 1);

    float u0_w = cv0.u * inv_w0, u1_w = cv1.u * inv_w1, u2_w = cv2.u * inv_w2;
    float v0_w = cv0.v * inv_w0, v1_w = cv1.v * inv_w1, v2_w = cv2.v * inv_w2;

    /* World position / w for perspective-correct interpolation */
    float wx0_w = cv0.wx * inv_w0, wx1_w = cv1.wx * inv_w1, wx2_w = cv2.wx * inv_w2;
    float wy0_w = cv0.wy * inv_w0, wy1_w = cv1.wy * inv_w1, wy2_w = cv2.wy * inv_w2;
    float wz0_w = cv0.wz * inv_w0, wz1_w = cv1.wz * inv_w1, wz2_w = cv2.wz * inv_w2;
    /* World normal / w */
    float wnx0_w = cv0.wnx * inv_w0, wnx1_w = cv1.wnx * inv_w1, wnx2_w = cv2.wnx * inv_w2;
    float wny0_w = cv0.wny * inv_w0, wny1_w = cv1.wny * inv_w1, wny2_w = cv2.wny * inv_w2;
    float wnz0_w = cv0.wnz * inv_w0, wnz1_w = cv1.wnz * inv_w1, wnz2_w = cv2.wnz * inv_w2;

    bool do_fog = sr_fog_cfg.enabled;
    float fog_range_inv = 0.0f;
    if (do_fog && sr_fog_cfg.far_dist > sr_fog_cfg.near_dist)
        fog_range_inv = 1.0f / (sr_fog_cfg.far_dist - sr_fog_cfg.near_dist);

    for (int py = min_y; py <= max_y; py++) {
        for (int px = min_x; px <= max_x; px++) {
            float ppx = px + 0.5f;
            float ppy = py + 0.5f;

            float w0 = edge_func(sx1, sy1, sx2, sy2, ppx, ppy);
            float w1 = edge_func(sx2, sy2, sx0, sy0, ppx, ppy);
            float w2 = edge_func(sx0, sy0, sx1, sy1, ppx, ppy);
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

            w0 *= inv_area; w1 *= inv_area; w2 *= inv_area;

            float z = nz0 * w0 + nz1 * w1 + nz2 * w2;
            int idx = py * W + px;
            if (z >= fb->depth[idx]) continue;
            fb->depth[idx] = z;

            float iw = inv_w0 * w0 + inv_w1 * w1 + inv_w2 * w2;
            float inv_iw = 1.0f / iw;
            float u = (u0_w * w0 + u1_w * w1 + u2_w * w2) * inv_iw;
            float v = (v0_w * w0 + v1_w * w1 + v2_w * w2) * inv_iw;

            /* Sample indexed texture */
            uint8_t pal_idx = 0;
            if (tex && tex->indices)
                pal_idx = sr_indexed_sample(tex, u, v);

            /* Interpolate world position and normal (perspective-correct) */
            float wpx = (wx0_w * w0 + wx1_w * w1 + wx2_w * w2) * inv_iw;
            float wpy = (wy0_w * w0 + wy1_w * w1 + wy2_w * w2) * inv_iw;
            float wpz = (wz0_w * w0 + wz1_w * w1 + wz2_w * w2) * inv_iw;
            float wnx = (wnx0_w * w0 + wnx1_w * w1 + wnx2_w * w2) * inv_iw;
            float wny = (wny0_w * w0 + wny1_w * w1 + wny2_w * w2) * inv_iw;
            float wnz = (wnz0_w * w0 + wnz1_w * w1 + wnz2_w * w2) * inv_iw;

            /* Per-pixel lighting via callback */
            float intensity = s_pixel_light_fn(wpx, wpy, wpz, wnx, wny, wnz);
            int shade_row = sr_shade_row_dithered(intensity, px, py);

            uint32_t color = sr_palette_lookup(shade_row, pal_idx);

            /* Bayer-dithered fog */
            if (do_fog) {
                float linear_depth = inv_iw;
                float fog_factor = (linear_depth - sr_fog_cfg.near_dist) * fog_range_inv;
                fog_factor = clampf(fog_factor, 0.0f, 1.0f);
                float threshold = (float)bayer4x4[py & 3][px & 3] * (1.0f / 16.0f);
                if (fog_factor > threshold)
                    color = sr_fog_cfg.color;
            }

            fb->color[idx] = color;
        }
    }
}

void sr_draw_triangle_indexed_pixellit(sr_framebuffer *fb,
                                        sr_vertex v0, sr_vertex v1, sr_vertex v2,
                                        const sr_indexed_texture *tex,
                                        const sr_mat4 *mvp)
{
    tri_count++;

    clip_vertex tri[3];
    fill_clip_vertex(&tri[0], &v0, *mvp);
    fill_clip_vertex(&tri[1], &v1, *mvp);
    fill_clip_vertex(&tri[2], &v2, *mvp);

    if (tri[0].clip.w < NEAR_W && tri[1].clip.w < NEAR_W && tri[2].clip.w < NEAR_W)
        return;

    if (tri[0].clip.w >= NEAR_W && tri[1].clip.w >= NEAR_W && tri[2].clip.w >= NEAR_W) {
        rasterize_triangle_indexed_pixellit(fb, tri[0], tri[1], tri[2], tex);
        return;
    }

    clip_vertex clipped[4];
    int n = clip_near_plane(tri, 3, clipped);
    for (int i = 1; i + 1 < n; i++)
        rasterize_triangle_indexed_pixellit(fb, clipped[0], clipped[i], clipped[i + 1], tex);
}

void sr_draw_quad_indexed_pixellit(sr_framebuffer *fb,
                                    sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                                    const sr_indexed_texture *tex,
                                    const sr_mat4 *mvp)
{
    sr_draw_triangle_indexed_pixellit(fb, v0, v1, v2, tex, mvp);
    sr_draw_triangle_indexed_pixellit(fb, v0, v2, v3, tex, mvp);
}

void sr_draw_quad_indexed_doublesided_pixellit(sr_framebuffer *fb,
                                                sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                                                const sr_indexed_texture *tex,
                                                const sr_mat4 *mvp)
{
    sr_draw_quad_indexed_pixellit(fb, v0, v1, v2, v3, tex, mvp);
    sr_draw_quad_indexed_pixellit(fb, v0, v3, v2, v1, tex, mvp);
}

/* ── Depth-only rasterization (for shadow maps) ──────────────────── */

static void rasterize_triangle_depth_only(sr_framebuffer *fb,
                                           clip_vertex cv0, clip_vertex cv1, clip_vertex cv2)
{
    int W = fb->width;
    int H = fb->height;

    float inv_w0 = 1.0f / cv0.clip.w;
    float inv_w1 = 1.0f / cv1.clip.w;
    float inv_w2 = 1.0f / cv2.clip.w;

    float nz0 = cv0.clip.z * inv_w0;
    float nz1 = cv1.clip.z * inv_w1;
    float nz2 = cv2.clip.z * inv_w2;

    float sx0 = (cv0.clip.x * inv_w0 + 1.0f) * 0.5f * W;
    float sy0 = (1.0f - cv0.clip.y * inv_w0) * 0.5f * H;
    float sx1 = (cv1.clip.x * inv_w1 + 1.0f) * 0.5f * W;
    float sy1 = (1.0f - cv1.clip.y * inv_w1) * 0.5f * H;
    float sx2 = (cv2.clip.x * inv_w2 + 1.0f) * 0.5f * W;
    float sy2 = (1.0f - cv2.clip.y * inv_w2) * 0.5f * H;

    /* No backface culling for shadow maps — both sides cast shadows */
    float area = edge_func(sx0, sy0, sx1, sy1, sx2, sy2);
    if (area == 0.0f) return;
    float inv_area = 1.0f / fabsf(area);
    bool flipped = (area < 0.0f);

    int min_x = clampi((int)floorf(fminf3(sx0, sx1, sx2)), 0, W - 1);
    int max_x = clampi((int)ceilf(fmaxf3(sx0, sx1, sx2)),  0, W - 1);
    int min_y = clampi((int)floorf(fminf3(sy0, sy1, sy2)), 0, H - 1);
    int max_y = clampi((int)ceilf(fmaxf3(sy0, sy1, sy2)),  0, H - 1);

    for (int py = min_y; py <= max_y; py++) {
        for (int px = min_x; px <= max_x; px++) {
            float ppx = px + 0.5f;
            float ppy = py + 0.5f;

            float w0 = edge_func(sx1, sy1, sx2, sy2, ppx, ppy);
            float w1 = edge_func(sx2, sy2, sx0, sy0, ppx, ppy);
            float w2 = edge_func(sx0, sy0, sx1, sy1, ppx, ppy);

            if (flipped) { w0 = -w0; w1 = -w1; w2 = -w2; }
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

            w0 *= inv_area; w1 *= inv_area; w2 *= inv_area;

            float z = nz0 * w0 + nz1 * w1 + nz2 * w2;
            int idx = py * W + px;
            if (z >= fb->depth[idx]) continue;
            fb->depth[idx] = z;
        }
    }
}

void sr_draw_triangle_depth_only(sr_framebuffer *fb,
                                  sr_vertex v0, sr_vertex v1, sr_vertex v2,
                                  const sr_mat4 *mvp)
{
    clip_vertex tri[3];
    fill_clip_vertex(&tri[0], &v0, *mvp);
    fill_clip_vertex(&tri[1], &v1, *mvp);
    fill_clip_vertex(&tri[2], &v2, *mvp);

    if (tri[0].clip.w < NEAR_W && tri[1].clip.w < NEAR_W && tri[2].clip.w < NEAR_W)
        return;

    if (tri[0].clip.w >= NEAR_W && tri[1].clip.w >= NEAR_W && tri[2].clip.w >= NEAR_W) {
        rasterize_triangle_depth_only(fb, tri[0], tri[1], tri[2]);
        return;
    }

    clip_vertex clipped[4];
    int n = clip_near_plane(tri, 3, clipped);
    for (int i = 1; i + 1 < n; i++)
        rasterize_triangle_depth_only(fb, clipped[0], clipped[i], clipped[i + 1]);
}

void sr_draw_quad_depth_only(sr_framebuffer *fb,
                              sr_vertex v0, sr_vertex v1, sr_vertex v2, sr_vertex v3,
                              const sr_mat4 *mvp)
{
    sr_draw_triangle_depth_only(fb, v0, v1, v2, mvp);
    sr_draw_triangle_depth_only(fb, v0, v2, v3, mvp);
}
