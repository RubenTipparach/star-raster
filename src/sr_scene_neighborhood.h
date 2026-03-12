/*  sr_scene_neighborhood.h — Neighborhood scene + shared draw_cube.
 *  Single-TU header-only. Depends on sr_lighting.h, sr_app.h. */
#ifndef SR_SCENE_NEIGHBORHOOD_H
#define SR_SCENE_NEIGHBORHOOD_H

/* ── Unit cube (6 faces = 12 triangles) ──────────────────────────── */

static void draw_cube(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                      const sr_texture *tex)
{
    float s = 0.5f;
    sr_draw_quad(fb_ptr,
        sr_vert(-s, -s,  s,  0, 1), sr_vert(-s,  s,  s,  0, 0),
        sr_vert( s,  s,  s,  1, 0), sr_vert( s, -s,  s,  1, 1), tex, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert( s, -s, -s,  0, 1), sr_vert( s,  s, -s,  0, 0),
        sr_vert(-s,  s, -s,  1, 0), sr_vert(-s, -s, -s,  1, 1), tex, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert(-s, -s, -s,  0, 1), sr_vert(-s,  s, -s,  0, 0),
        sr_vert(-s,  s,  s,  1, 0), sr_vert(-s, -s,  s,  1, 1), tex, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert( s, -s,  s,  0, 1), sr_vert( s,  s,  s,  0, 0),
        sr_vert( s,  s, -s,  1, 0), sr_vert( s, -s, -s,  1, 1), tex, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert(-s,  s,  s,  0, 1), sr_vert(-s,  s, -s,  0, 0),
        sr_vert( s,  s, -s,  1, 0), sr_vert( s,  s,  s,  1, 1), tex, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert(-s, -s, -s,  0, 1), sr_vert(-s, -s,  s,  0, 0),
        sr_vert( s, -s,  s,  1, 0), sr_vert( s, -s, -s,  1, 1), tex, mvp);
}

/* ── Single house (drawn relative to model origin) ───────────────── */

static sr_vertex lit_house_vert(float lx, float ly, float lz, float u, float v,
                                float nx, float ny, float nz,
                                float hx, float hz, float rot_y) {
    float c = cosf(rot_y), s = sinf(rot_y);
    float wx = lx*c + lz*s + hx;
    float wy = ly;
    float wz = -lx*s + lz*c + hz;
    uint32_t col = vertex_light_rotY(wx, wy, wz, nx, ny, nz, rot_y);
    return (sr_vertex){ {lx, ly, lz}, {u, v}, col };
}

static void draw_house(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                        float hx, float hz, float rot_y) {
    float W = 2.0f, D = 1.5f, WH = 2.0f, RP = 3.0f;

    #define LV(lx,ly,lz,u,v,nx,ny,nz) lit_house_vert(lx,ly,lz,u,v,nx,ny,nz,hx,hz,rot_y)

    float slope = (RP - WH) / W;
    float rn_len = sqrtf(1.0f + slope * slope);
    float rnx = 1.0f / rn_len, rny = slope / rn_len;

    /* Back wall */
    sr_draw_quad(fb_ptr,
        LV(-W, 0,  -D,  0,   WH/2,  0,0,-1),
        LV( W, 0,  -D,  W,   WH/2,  0,0,-1),
        LV( W, WH, -D,  W,   0,     0,0,-1),
        LV(-W, WH, -D,  0,   0,     0,0,-1),
        &textures[TEX_BRICK], mvp);

    /* Left wall */
    sr_draw_quad(fb_ptr,
        LV(-W, 0,   D,  0,    WH/2, -1,0,0),
        LV(-W, 0,  -D,  D,    WH/2, -1,0,0),
        LV(-W, WH, -D,  D,    0,    -1,0,0),
        LV(-W, WH,  D,  0,    0,    -1,0,0),
        &textures[TEX_BRICK], mvp);

    /* Right wall */
    sr_draw_quad(fb_ptr,
        LV( W, 0,  -D,  0,    WH/2,  1,0,0),
        LV( W, 0,   D,  D,    WH/2,  1,0,0),
        LV( W, WH,  D,  D,    0,     1,0,0),
        LV( W, WH, -D,  0,    0,     1,0,0),
        &textures[TEX_BRICK], mvp);

    /* Front wall with door */
    {
        float dhw = 0.4f, dh = 1.6f;

        sr_draw_quad(fb_ptr,
            LV(-dhw, 0,  D,  0,            WH/2, 0,0,1),
            LV(-W,   0,  D,  (W-dhw)/2,    WH/2, 0,0,1),
            LV(-W,   WH, D,  (W-dhw)/2,    0,    0,0,1),
            LV(-dhw, WH, D,  0,            0,    0,0,1),
            &textures[TEX_BRICK], mvp);

        sr_draw_quad(fb_ptr,
            LV( W,   0,  D,  0,            WH/2, 0,0,1),
            LV( dhw, 0,  D,  (W-dhw)/2,    WH/2, 0,0,1),
            LV( dhw, WH, D,  (W-dhw)/2,    0,    0,0,1),
            LV( W,   WH, D,  0,            0,    0,0,1),
            &textures[TEX_BRICK], mvp);

        sr_draw_quad(fb_ptr,
            LV( dhw, dh, D,  0,      (WH-dh)/2, 0,0,1),
            LV(-dhw, dh, D,  dhw,    (WH-dh)/2, 0,0,1),
            LV(-dhw, WH, D,  dhw,    0,         0,0,1),
            LV( dhw, WH, D,  0,      0,         0,0,1),
            &textures[TEX_BRICK], mvp);

        sr_draw_quad(fb_ptr,
            LV( dhw, 0,  D+0.01f, 0,1, 0,0,1),
            LV(-dhw, 0,  D+0.01f, 1,1, 0,0,1),
            LV(-dhw, dh, D+0.01f, 1,0, 0,0,1),
            LV( dhw, dh, D+0.01f, 0,0, 0,0,1),
            &textures[TEX_WOOD], mvp);
    }

    /* Gable ends */
    sr_draw_triangle(fb_ptr,
        LV( W, WH, D,  0,    1, 0,0,1),
        LV(-W, WH, D,  1,    1, 0,0,1),
        LV( 0, RP, D,  0.5f, 0, 0,0,1),
        &textures[TEX_BRICK], mvp);

    sr_draw_triangle(fb_ptr,
        LV(-W, WH, -D, 0,    1, 0,0,-1),
        LV( W, WH, -D, 1,    1, 0,0,-1),
        LV( 0, RP, -D, 0.5f, 0, 0,0,-1),
        &textures[TEX_BRICK], mvp);

    /* Roof slopes (double-sided) */
    {
        float oh = 0.3f;
        float ey = WH - oh * slope;

        sr_draw_quad_doublesided(fb_ptr,
            LV( 0,    RP, -D-oh,  0,    1, -rnx,rny,0),
            LV( 0,    RP,  D+oh,  1.5f, 1, -rnx,rny,0),
            LV(-W-oh, ey,  D+oh,  1.5f, 0, -rnx,rny,0),
            LV(-W-oh, ey, -D-oh,  0,    0, -rnx,rny,0),
            &textures[TEX_ROOF], mvp);

        sr_draw_quad_doublesided(fb_ptr,
            LV( 0,    RP,  D+oh,  0,    1,  rnx,rny,0),
            LV( 0,    RP, -D-oh,  1.5f, 1,  rnx,rny,0),
            LV( W+oh, ey, -D-oh,  1.5f, 0,  rnx,rny,0),
            LV( W+oh, ey,  D+oh,  0,    0,  rnx,rny,0),
            &textures[TEX_ROOF], mvp);
    }

    /* Windows */
    {
        float wy = 1.0f, wh = 0.6f, whw = 0.35f, wz = 0.0f;

        sr_draw_quad(fb_ptr,
            LV(-W-0.01f, wy,    wz+whw, 0,1, -1,0,0),
            LV(-W-0.01f, wy,    wz-whw, 1,1, -1,0,0),
            LV(-W-0.01f, wy+wh, wz-whw, 1,0, -1,0,0),
            LV(-W-0.01f, wy+wh, wz+whw, 0,0, -1,0,0),
            &textures[TEX_TILE], mvp);

        sr_draw_quad(fb_ptr,
            LV(W+0.01f, wy,    wz-whw, 0,1, 1,0,0),
            LV(W+0.01f, wy,    wz+whw, 1,1, 1,0,0),
            LV(W+0.01f, wy+wh, wz+whw, 1,0, 1,0,0),
            LV(W+0.01f, wy+wh, wz-whw, 0,0, 1,0,0),
            &textures[TEX_TILE], mvp);
    }

    #undef LV
}

/* ── Neighborhood layout ─────────────────────────────────────────── */

typedef struct { float x, z, rot; } house_placement;

static const house_placement houses[] = {
    { -12.0f, -14.0f,  0.0f },
    {   0.0f, -14.0f,  0.0f },
    {  12.0f, -14.0f,  0.0f },
    { -12.0f,  -6.0f,  3.14159f },
    {   0.0f,  -6.0f,  3.14159f },
    {  12.0f,  -6.0f,  3.14159f },
    { -12.0f,   6.0f,  0.0f },
    {   0.0f,   6.0f,  0.0f },
    {  12.0f,   6.0f,  0.0f },
    { -12.0f,  14.0f,  3.14159f },
    {   0.0f,  14.0f,  3.14159f },
    {  12.0f,  14.0f,  3.14159f },
};
#define NUM_HOUSES (sizeof(houses) / sizeof(houses[0]))

static void draw_ground_quad(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                              float x0, float z0, float x1, float z1,
                              float u0, float v0, float u1, float v1,
                              float y, const sr_texture *tex) {
    uint32_t c00 = vertex_light(x0, y, z0, 0,1,0);
    uint32_t c10 = vertex_light(x1, y, z0, 0,1,0);
    uint32_t c11 = vertex_light(x1, y, z1, 0,1,0);
    uint32_t c01 = vertex_light(x0, y, z1, 0,1,0);
    sr_draw_quad(fb_ptr,
        sr_vert_c(x0, y, z1,  u0, v1, c01),
        sr_vert_c(x0, y, z0,  u0, v0, c00),
        sr_vert_c(x1, y, z0,  u1, v0, c10),
        sr_vert_c(x1, y, z1,  u1, v1, c11),
        tex, vp);
}

static void draw_street_tiled(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                               float x0, float z0, float x1, float z1,
                               float y, float tile) {
    for (float tx = x0; tx < x1; tx += tile) {
        float nx = tx + tile;
        if (nx > x1) nx = x1;
        for (float tz = z0; tz < z1; tz += tile) {
            float nz = tz + tile;
            if (nz > z1) nz = z1;
            draw_ground_quad(fb_ptr, vp, tx, tz, nx, nz,
                             0, 0, (nx-tx)*0.5f, (nz-tz)*0.5f, y, &textures[TEX_TILE]);
        }
    }
}

/* ── Street light setup (night mode) ─────────────────────────────── */

static void setup_street_lights(float t) {
    num_lights = 0;
    if (!night_mode) return;

    float hz[] = { -10.0f, 0.0f, 10.0f };
    for (int i = 0; i < 3; i++) {
        float speed = 6.0f;
        float range = 22.0f;
        float x1 = fmodf(t * speed + i * 8.0f, range * 2.0f) - range;
        float x2 = fmodf(-t * speed + i * 5.0f + 15.0f, range * 2.0f) - range;
        lights[num_lights++] = (point_light){ x1, 3.0f, hz[i], 1.0f, 0.85f, 0.5f, 12.0f };
        lights[num_lights++] = (point_light){ x2, 3.0f, hz[i], 1.0f, 0.85f, 0.5f, 12.0f };
    }

    float vx[] = { -6.0f, 6.0f };
    for (int i = 0; i < 2; i++) {
        float speed = 5.0f;
        float range = 20.0f;
        float z1 = fmodf(t * speed + i * 12.0f, range * 2.0f) - range;
        float z2 = fmodf(-t * speed + i * 7.0f + 10.0f, range * 2.0f) - range;
        lights[num_lights++] = (point_light){ vx[i], 3.0f, z1, 0.8f, 0.7f, 1.0f, 10.0f };
        lights[num_lights++] = (point_light){ vx[i], 3.0f, z2, 0.8f, 0.7f, 1.0f, 10.0f };
    }
}

static void draw_neighborhood(sr_framebuffer *fb_ptr, const sr_mat4 *vp) {
    /* Ground plane (grass) — tiled */
    {
        float G = 30.0f;
        int   tiles = 10;
        float ts = (2.0f * G) / tiles;
        for (int tz = 0; tz < tiles; tz++) {
            for (int tx = 0; tx < tiles; tx++) {
                float x0 = -G + tx * ts, x1 = x0 + ts;
                float z0 = -G + tz * ts, z1 = z0 + ts;
                float u0 = (x0 + G) * 0.5f, u1 = (x1 + G) * 0.5f;
                float v0 = (z0 + G) * 0.5f, v1 = (z1 + G) * 0.5f;
                draw_ground_quad(fb_ptr, vp, x0, z0, x1, z1,
                                 u0, v0, u1, v1, 0.0f, &textures[TEX_GRASS]);
            }
        }
    }

    /* Streets (tiled, no overlap at intersections) */
    float sw = 1.2f;
    float sy = 0.05f;
    float tile = 2.4f;
    float h_len = 25.0f;
    float v_len = 22.0f;
    float hz[] = { -10.0f, 0.0f, 10.0f };
    float vx[] = {  -6.0f, 6.0f };

    for (int i = 0; i < 3; i++) {
        draw_street_tiled(fb_ptr, vp, -h_len, hz[i]-sw, h_len, hz[i]+sw,
                          sy, tile);
    }

    for (int i = 0; i < 2; i++) {
        float segs[][2] = {
            { -v_len,      hz[0] - sw },
            { hz[0] + sw,  hz[1] - sw },
            { hz[1] + sw,  hz[2] - sw },
            { hz[2] + sw,  v_len      },
        };
        for (int j = 0; j < 4; j++) {
            draw_street_tiled(fb_ptr, vp, vx[i]-sw, segs[j][0], vx[i]+sw, segs[j][1],
                              sy, tile);
        }
    }

    /* Houses */
    for (int i = 0; i < (int)NUM_HOUSES; i++) {
        sr_mat4 model = sr_mat4_mul(
            sr_mat4_translate(houses[i].x, 0, houses[i].z),
            sr_mat4_rotate_y(houses[i].rot)
        );
        sr_mat4 mvp = sr_mat4_mul(*vp, model);
        draw_house(fb_ptr, &mvp, houses[i].x, houses[i].z, houses[i].rot);
    }
}

#endif /* SR_SCENE_NEIGHBORHOOD_H */
