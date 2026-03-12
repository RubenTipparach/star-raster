/*  sr_scene_palette.h — Palette House scene (indexed textures, pixel lighting).
 *  Single-TU header-only. Depends on sr_lighting.h, sr_app.h. */
#ifndef SR_SCENE_PALETTE_H
#define SR_SCENE_PALETTE_H

static sr_vertex pal_house_vert(float lx, float ly, float lz, float u, float v,
                                 float nx, float ny, float nz,
                                 float hx, float hz, float rot_y) {
    float c = cosf(rot_y), s = sinf(rot_y);
    float wx = lx*c + lz*s + hx;
    float wy = ly;
    float wz = -lx*s + lz*c + hz;
    float wnx = nx*c + nz*s, wny = ny, wnz = -nx*s + nz*c;
    float intensity = pal_vertex_intensity(wx, wy, wz, wnx, wny, wnz);
    uint32_t col = pal_intensity_color(intensity);
    return sr_vert_world(lx, ly, lz, u, v, col, wx, wy, wz, wnx, wny, wnz);
}

static void draw_palette_house(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                float hx, float hz, float rot_y) {
    float W = 2.0f, D = 1.5f, WH = 2.0f, RP = 3.0f;

    #define PV(lx,ly,lz,u,v,nx,ny,nz) pal_house_vert(lx,ly,lz,u,v,nx,ny,nz,hx,hz,rot_y)

    void (*draw_quad)(sr_framebuffer*, sr_vertex, sr_vertex, sr_vertex, sr_vertex,
                      const sr_indexed_texture*, const sr_mat4*) =
        pixel_lighting ? sr_draw_quad_indexed_pixellit : sr_draw_quad_indexed;
    void (*draw_tri)(sr_framebuffer*, sr_vertex, sr_vertex, sr_vertex,
                     const sr_indexed_texture*, const sr_mat4*) =
        pixel_lighting ? sr_draw_triangle_indexed_pixellit : sr_draw_triangle_indexed;
    void (*draw_quad_ds)(sr_framebuffer*, sr_vertex, sr_vertex, sr_vertex, sr_vertex,
                         const sr_indexed_texture*, const sr_mat4*) =
        pixel_lighting ? sr_draw_quad_indexed_doublesided_pixellit : sr_draw_quad_indexed_doublesided;

    float slope = (RP - WH) / W;
    float rn_len = sqrtf(1.0f + slope * slope);
    float rnx = 1.0f / rn_len, rny = slope / rn_len;

    /* Back wall */
    draw_quad(fb_ptr,
        PV(-W, 0,  -D,  0,   WH/2,  0,0,-1),
        PV( W, 0,  -D,  W,   WH/2,  0,0,-1),
        PV( W, WH, -D,  W,   0,     0,0,-1),
        PV(-W, WH, -D,  0,   0,     0,0,-1),
        &itextures[ITEX_BRICK], mvp);

    /* Left wall */
    draw_quad(fb_ptr,
        PV(-W, 0,   D,  0,    WH/2, -1,0,0),
        PV(-W, 0,  -D,  D,    WH/2, -1,0,0),
        PV(-W, WH, -D,  D,    0,    -1,0,0),
        PV(-W, WH,  D,  0,    0,    -1,0,0),
        &itextures[ITEX_BRICK], mvp);

    /* Right wall */
    draw_quad(fb_ptr,
        PV( W, 0,  -D,  0,    WH/2,  1,0,0),
        PV( W, 0,   D,  D,    WH/2,  1,0,0),
        PV( W, WH,  D,  D,    0,     1,0,0),
        PV( W, WH, -D,  0,    0,     1,0,0),
        &itextures[ITEX_BRICK], mvp);

    /* Front wall */
    draw_quad(fb_ptr,
        PV( W, 0,   D,  0,   WH/2, 0,0,1),
        PV(-W, 0,   D,  W,   WH/2, 0,0,1),
        PV(-W, WH,  D,  W,   0,    0,0,1),
        PV( W, WH,  D,  0,   0,    0,0,1),
        &itextures[ITEX_BRICK], mvp);

    /* Door */
    {
        float dhw = 0.4f, dh = 1.6f;
        draw_quad(fb_ptr,
            PV( dhw, 0,  D+0.01f, 0,1, 0,0,1),
            PV(-dhw, 0,  D+0.01f, 1,1, 0,0,1),
            PV(-dhw, dh, D+0.01f, 1,0, 0,0,1),
            PV( dhw, dh, D+0.01f, 0,0, 0,0,1),
            &itextures[ITEX_WOOD], mvp);
    }

    /* Gable ends */
    draw_tri(fb_ptr,
        PV( W, WH, D,  0,    1, 0,0,1),
        PV(-W, WH, D,  1,    1, 0,0,1),
        PV( 0, RP, D,  0.5f, 0, 0,0,1),
        &itextures[ITEX_BRICK], mvp);

    draw_tri(fb_ptr,
        PV(-W, WH, -D, 0,    1, 0,0,-1),
        PV( W, WH, -D, 1,    1, 0,0,-1),
        PV( 0, RP, -D, 0.5f, 0, 0,0,-1),
        &itextures[ITEX_BRICK], mvp);

    /* Roof slopes */
    {
        float oh = 0.3f;
        float ey = WH - oh * slope;

        draw_quad_ds(fb_ptr,
            PV( 0,    RP, -D-oh,  0,    1, -rnx,rny,0),
            PV( 0,    RP,  D+oh,  1.5f, 1, -rnx,rny,0),
            PV(-W-oh, ey,  D+oh,  1.5f, 0, -rnx,rny,0),
            PV(-W-oh, ey, -D-oh,  0,    0, -rnx,rny,0),
            &itextures[ITEX_ROOF], mvp);

        draw_quad_ds(fb_ptr,
            PV( 0,    RP,  D+oh,  0,    1,  rnx,rny,0),
            PV( 0,    RP, -D-oh,  1.5f, 1,  rnx,rny,0),
            PV( W+oh, ey, -D-oh,  1.5f, 0,  rnx,rny,0),
            PV( W+oh, ey,  D+oh,  0,    0,  rnx,rny,0),
            &itextures[ITEX_ROOF], mvp);
    }

    /* Windows */
    {
        float wy = 1.0f, wh = 0.6f, whw = 0.35f, wz = 0.0f;

        draw_quad(fb_ptr,
            PV(-W-0.01f, wy,    wz+whw, 0,1, -1,0,0),
            PV(-W-0.01f, wy,    wz-whw, 1,1, -1,0,0),
            PV(-W-0.01f, wy+wh, wz-whw, 1,0, -1,0,0),
            PV(-W-0.01f, wy+wh, wz+whw, 0,0, -1,0,0),
            &itextures[ITEX_TILE], mvp);

        draw_quad(fb_ptr,
            PV(W+0.01f, wy,    wz-whw, 0,1, 1,0,0),
            PV(W+0.01f, wy,    wz+whw, 1,1, 1,0,0),
            PV(W+0.01f, wy+wh, wz+whw, 1,0, 1,0,0),
            PV(W+0.01f, wy+wh, wz-whw, 0,0, 1,0,0),
            &itextures[ITEX_TILE], mvp);
    }

    #undef PV
}

/* ── Wireframe sphere ────────────────────────────────────────────── */

static void draw_line_3d(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                          float x0, float y0, float z0,
                          float x1, float y1, float z1, uint32_t color)
{
    int W = fb_ptr->width, H = fb_ptr->height;
    sr_vec4 c0 = sr_mat4_mul_v4(*mvp, (sr_vec4){x0, y0, z0, 1.0f});
    sr_vec4 c1 = sr_mat4_mul_v4(*mvp, (sr_vec4){x1, y1, z1, 1.0f});

    if (c0.w < 0.001f && c1.w < 0.001f) return;
    if (c0.w < 0.001f || c1.w < 0.001f) return;

    float sx0 = (c0.x / c0.w + 1.0f) * 0.5f * W;
    float sy0 = (1.0f - c0.y / c0.w) * 0.5f * H;
    float sx1 = (c1.x / c1.w + 1.0f) * 0.5f * W;
    float sy1 = (1.0f - c1.y / c1.w) * 0.5f * H;

    int ix0 = (int)sx0, iy0 = (int)sy0;
    int ix1 = (int)sx1, iy1 = (int)sy1;

    int dx = abs(ix1 - ix0), dy = -abs(iy1 - iy0);
    int step_x = ix0 < ix1 ? 1 : -1;
    int step_y = iy0 < iy1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        if (ix0 >= 0 && ix0 < W && iy0 >= 0 && iy0 < H)
            fb_ptr->color[iy0 * W + ix0] = color;
        if (ix0 == ix1 && iy0 == iy1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; ix0 += step_x; }
        if (e2 <= dx) { err += dx; iy0 += step_y; }
    }
}

static void draw_wireframe_sphere(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                    float cx, float cy, float cz, float radius,
                                    uint32_t color, int segments)
{
    float pi = 3.14159265f;
    for (int i = 0; i < segments; i++) {
        float a0 = 2.0f * pi * i / segments;
        float a1 = 2.0f * pi * (i + 1) / segments;
        float c0a = cosf(a0), s0a = sinf(a0);
        float c1a = cosf(a1), s1a = sinf(a1);

        draw_line_3d(fb_ptr, vp,
            cx + radius * c0a, cy, cz + radius * s0a,
            cx + radius * c1a, cy, cz + radius * s1a, color);
        draw_line_3d(fb_ptr, vp,
            cx + radius * c0a, cy + radius * s0a, cz,
            cx + radius * c1a, cy + radius * s1a, cz, color);
        draw_line_3d(fb_ptr, vp,
            cx, cy + radius * c0a, cz + radius * s0a,
            cx, cy + radius * c1a, cz + radius * s1a, color);
    }
}

/* ── Shadow pass (depth-only) ────────────────────────────────────── */

static void draw_palette_scene_depth_only(sr_framebuffer *sm, const sr_mat4 *lvp) {
    /* Ground plane */
    {
        float G = 10.0f;
        int tiles = 6;
        float ts = (2.0f * G) / tiles;
        for (int tz = 0; tz < tiles; tz++) {
            for (int tx = 0; tx < tiles; tx++) {
                float x0 = -G + tx * ts, x1 = x0 + ts;
                float z0 = -G + tz * ts, z1 = z0 + ts;
                sr_draw_quad_depth_only(sm,
                    sr_vert(x0, 0, z1, 0, 0),
                    sr_vert(x0, 0, z0, 0, 0),
                    sr_vert(x1, 0, z0, 0, 0),
                    sr_vert(x1, 0, z1, 0, 0),
                    lvp);
            }
        }
    }
    /* House at origin */
    {
        float W = 2.0f, D = 1.5f, WH = 2.0f, RP = 3.0f;
        float slope = (RP - WH) / W;
        float oh = 0.3f, ey = WH - oh * slope;
        #define DV(x,y,z) sr_vert(x,y,z, 0,0)

        sr_draw_quad_depth_only(sm, DV(-W,0,-D), DV(W,0,-D), DV(W,WH,-D), DV(-W,WH,-D), lvp);
        sr_draw_quad_depth_only(sm, DV(-W,0,D), DV(-W,0,-D), DV(-W,WH,-D), DV(-W,WH,D), lvp);
        sr_draw_quad_depth_only(sm, DV(W,0,-D), DV(W,0,D), DV(W,WH,D), DV(W,WH,-D), lvp);
        sr_draw_quad_depth_only(sm, DV(W,0,D), DV(-W,0,D), DV(-W,WH,D), DV(W,WH,D), lvp);

        sr_draw_triangle_depth_only(sm, DV(W,WH,D), DV(-W,WH,D), DV(0,RP,D), lvp);
        sr_draw_triangle_depth_only(sm, DV(-W,WH,-D), DV(W,WH,-D), DV(0,RP,-D), lvp);

        sr_draw_quad_depth_only(sm, DV(0,RP,-D-oh), DV(0,RP,D+oh), DV(-W-oh,ey,D+oh), DV(-W-oh,ey,-D-oh), lvp);
        sr_draw_quad_depth_only(sm, DV(0,RP,D+oh), DV(0,RP,-D-oh), DV(W+oh,ey,-D-oh), DV(W+oh,ey,D+oh), lvp);

        #undef DV
    }
}

/* Pixel lighting callback */
static float pixel_light_callback(float px, float py, float pz,
                                   float nx, float ny, float nz) {
    return pal_vertex_intensity(px, py, pz, nx, ny, nz);
}

static void draw_palette_scene(sr_framebuffer *fb_ptr, const sr_mat4 *vp, float t) {
    num_lights = 1;
    float orbit_radius = 5.0f;
    float lx = cosf(t * 1.2f) * orbit_radius;
    float lz = sinf(t * 1.2f) * orbit_radius;
    float ly = 2.5f + sinf(t * 0.7f) * 1.0f;
    lights[0] = (point_light){ lx, ly, lz,
        2.0f * pal_light_mult, 1.8f * pal_light_mult, 1.2f * pal_light_mult, 12.0f };
    build_light_grid();

    /* Shadow map pass */
    if (shadows_enabled) {
        sr_mat4 light_view = sr_mat4_lookat(sr_v3(lx, ly, lz), sr_v3(0, 0, 0), sr_v3(0, 1, 0));
        sr_mat4 light_proj = sr_mat4_perspective(1.5f, 1.0f, 0.5f, 30.0f);
        light_vp = sr_mat4_mul(light_proj, light_view);
        sr_framebuffer_clear(&shadow_fb, 0, 1.0f);
        draw_palette_scene_depth_only(&shadow_fb, &light_vp);
    }

    if (pixel_lighting)
        sr_set_pixel_light_fn(pixel_light_callback);

    /* Ground plane (grass) */
    {
        float G = 10.0f;
        int tiles = 6;
        float ts = (2.0f * G) / tiles;
        for (int tz = 0; tz < tiles; tz++) {
            for (int tx = 0; tx < tiles; tx++) {
                float x0 = -G + tx * ts, x1 = x0 + ts;
                float z0 = -G + tz * ts, z1 = z0 + ts;

                float i00 = pal_vertex_intensity(x0, 0, z0, 0,1,0);
                float i10 = pal_vertex_intensity(x1, 0, z0, 0,1,0);
                float i11 = pal_vertex_intensity(x1, 0, z1, 0,1,0);
                float i01 = pal_vertex_intensity(x0, 0, z1, 0,1,0);

                if (pixel_lighting) {
                    sr_draw_quad_indexed_pixellit(fb_ptr,
                        sr_vert_world(x0, 0, z1, 0,ts, pal_intensity_color(i01), x0,0,z1, 0,1,0),
                        sr_vert_world(x0, 0, z0, 0, 0, pal_intensity_color(i00), x0,0,z0, 0,1,0),
                        sr_vert_world(x1, 0, z0, ts,0, pal_intensity_color(i10), x1,0,z0, 0,1,0),
                        sr_vert_world(x1, 0, z1, ts,ts, pal_intensity_color(i11), x1,0,z1, 0,1,0),
                        &itextures[ITEX_GRASS], vp);
                } else {
                    sr_draw_quad_indexed(fb_ptr,
                        sr_vert_c(x0, 0, z1, 0,ts, pal_intensity_color(i01)),
                        sr_vert_c(x0, 0, z0, 0, 0, pal_intensity_color(i00)),
                        sr_vert_c(x1, 0, z0, ts,0, pal_intensity_color(i10)),
                        sr_vert_c(x1, 0, z1, ts,ts, pal_intensity_color(i11)),
                        &itextures[ITEX_GRASS], vp);
                }
            }
        }
    }

    /* Single house at origin */
    sr_mat4 mvp = *vp;
    draw_palette_house(fb_ptr, &mvp, 0.0f, 0.0f, 0.0f);

    /* Draw wireframe sphere at light position */
    draw_wireframe_sphere(fb_ptr, vp, lx, ly, lz, 0.3f, 0xFF55CCFF, 16);
}

#endif /* SR_SCENE_PALETTE_H */
