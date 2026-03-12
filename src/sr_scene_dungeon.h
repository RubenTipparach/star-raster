/*  sr_scene_dungeon.h — Dungeon Crawler scene (rendering, config, game state).
 *  Single-TU header-only. Depends on sr_dungeon.h, sr_config.h, sr_lighting.h, sr_app.h. */
#ifndef SR_SCENE_DUNGEON_H
#define SR_SCENE_DUNGEON_H

#include "sr_config.h"

/* ── Runtime dungeon config (loaded from config/dungeon.yaml) ────── */

static struct {
    float light_color[3];
    float light_brightness;
    float light_min_range;
    float light_attn_dist;
    float ambient_color[3];
    float ambient_brightness;
    int   fog_levels;
    float fog_start[16];
    float fog_intensity[16];
    float fog_density[16];
    float fog_stop;
} dng_cfg;

static void dng_load_config(void) {
    sr_config cfg = sr_config_load("config/dungeon.yaml");

    float color[3] = {1,1,1};
    sr_config_array(&cfg, "torch.color", color, 3);
    dng_cfg.light_color[0] = color[0];
    dng_cfg.light_color[1] = color[1];
    dng_cfg.light_color[2] = color[2];
    dng_cfg.light_brightness = sr_config_float(&cfg, "torch.brightness", 1.0f);
    dng_cfg.light_min_range  = sr_config_float(&cfg, "torch.min_range", 1.0f);
    dng_cfg.light_attn_dist  = sr_config_float(&cfg, "torch.attn_dist", 6.0f);

    float amb[3] = {0.15f, 0.12f, 0.18f};
    sr_config_array(&cfg, "ambient.color", amb, 3);
    dng_cfg.ambient_color[0] = amb[0];
    dng_cfg.ambient_color[1] = amb[1];
    dng_cfg.ambient_color[2] = amb[2];
    dng_cfg.ambient_brightness = sr_config_float(&cfg, "ambient.brightness", 0.1f);

    dng_cfg.fog_levels = (int)sr_config_float(&cfg, "fog.levels", 5.0f);
    if (dng_cfg.fog_levels > 16) dng_cfg.fog_levels = 16;
    sr_config_array(&cfg, "fog.start", dng_cfg.fog_start, dng_cfg.fog_levels);
    sr_config_array(&cfg, "fog.intensity", dng_cfg.fog_intensity, dng_cfg.fog_levels);
    sr_config_array(&cfg, "fog.density", dng_cfg.fog_density, dng_cfg.fog_levels);
    dng_cfg.fog_stop = sr_config_float(&cfg, "fog.stop", 8.0f);

    sr_config_free(&cfg);
    printf("[dungeon] Config loaded: torch(%.1f/%.1f/%.1f) ambient(%.2f) fog(%d levels)\n",
           dng_cfg.light_brightness, dng_cfg.light_min_range, dng_cfg.light_attn_dist,
           dng_cfg.ambient_brightness, dng_cfg.fog_levels);
}

/* ── Game state ──────────────────────────────────────────────────── */

static dng_game dng_state;
static bool dng_initialized = false;

enum {
    DNG_STATE_PLAYING,
    DNG_STATE_CLIMBING,
};
static int dng_play_state = DNG_STATE_PLAYING;

static int dng_light_mode = 0;
static bool dng_show_info = false;

/* ── Torch lighting (pixel-lit callback) ─────────────────────────── */

static float dng_torch_light(float wx, float wy, float wz,
                              float nx, float ny, float nz)
{
    dng_player *p = &dng_state.player;

    float ambient = dng_cfg.ambient_brightness *
        (dng_cfg.ambient_color[0] + dng_cfg.ambient_color[1] + dng_cfg.ambient_color[2]) / 3.0f;

    float dx = p->x - wx;
    float dy = p->y - wy;
    float dz = p->z - wz;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz);

    if (dist >= dng_cfg.light_attn_dist) return ambient;

    float atten;
    if (dist <= dng_cfg.light_min_range) {
        atten = 1.0f;
    } else {
        atten = 1.0f - (dist - dng_cfg.light_min_range) /
                        (dng_cfg.light_attn_dist - dng_cfg.light_min_range);
        atten *= atten;
    }

    float inv_dist = 1.0f / (dist + 0.001f);
    float ndotl = (dx * nx + dy * ny + dz * nz) * inv_dist;
    if (ndotl < 0.0f) ndotl = 0.0f;

    float torch_lum = dng_cfg.light_brightness *
        (dng_cfg.light_color[0] + dng_cfg.light_color[1] + dng_cfg.light_color[2]) / 3.0f;

    return ambient + torch_lum * atten * ndotl;
}

/* ── Depth-fog intensity ─────────────────────────────────────────── */

static float dng_fog_intensity_at_dist(float dist_cells) {
    if (dist_cells < dng_cfg.fog_start[0]) return dng_cfg.fog_intensity[0];

    for (int i = 0; i < dng_cfg.fog_levels - 1; i++) {
        if (dist_cells < dng_cfg.fog_start[i + 1]) {
            float t = (dist_cells - dng_cfg.fog_start[i]) /
                      (dng_cfg.fog_start[i + 1] - dng_cfg.fog_start[i]);
            return dng_cfg.fog_intensity[i] + t * (dng_cfg.fog_intensity[i + 1] - dng_cfg.fog_intensity[i]);
        }
    }
    return dng_cfg.fog_intensity[dng_cfg.fog_levels - 1];
}

static float dng_fog_vertex_intensity(float wx, float wy, float wz) {
    dng_player *p = &dng_state.player;
    float dx = p->x - wx;
    float dy = p->y - wy;
    float dz = p->z - wz;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz) / DNG_CELL_SIZE;
    return dng_fog_intensity_at_dist(dist);
}

/* ── Wall / floor drawing ────────────────────────────────────────── */

static void dng_draw_wall(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                           float ax, float ay, float az,
                           float bx, float by, float bz,
                           float cx, float cy, float cz,
                           float dx, float dy, float dz,
                           const sr_indexed_texture *tex,
                           float nx, float ny, float nz) {
    float edge_x = bx - ax, edge_y = by - ay, edge_z = bz - az;
    float width = sqrtf(edge_x*edge_x + edge_y*edge_y + edge_z*edge_z);
    float vert_x = dx - ax, vert_y = dy - ay, vert_z = dz - az;
    float height = sqrtf(vert_x*vert_x + vert_y*vert_y + vert_z*vert_z);
    float u_scale = width / DNG_CELL_SIZE;
    float v_scale = height / DNG_CELL_SIZE;

    if (dng_light_mode == 0) {
        sr_draw_quad_indexed_pixellit(fb_ptr,
            sr_vert_world(ax,ay,az, 0,0, 0xFFFFFFFF, ax,ay,az, nx,ny,nz),
            sr_vert_world(bx,by,bz, u_scale,0, 0xFFFFFFFF, bx,by,bz, nx,ny,nz),
            sr_vert_world(cx,cy,cz, u_scale,v_scale, 0xFFFFFFFF, cx,cy,cz, nx,ny,nz),
            sr_vert_world(dx,dy,dz, 0,v_scale, 0xFFFFFFFF, dx,dy,dz, nx,ny,nz),
            tex, mvp);
    } else {
        uint32_t ca = pal_intensity_color(dng_fog_vertex_intensity(ax,ay,az));
        uint32_t cb = pal_intensity_color(dng_fog_vertex_intensity(bx,by,bz));
        uint32_t cc = pal_intensity_color(dng_fog_vertex_intensity(cx,cy,cz));
        uint32_t cd = pal_intensity_color(dng_fog_vertex_intensity(dx,dy,dz));
        sr_draw_quad_indexed(fb_ptr,
            sr_vert_c(ax,ay,az, 0,0, ca),
            sr_vert_c(bx,by,bz, u_scale,0, cb),
            sr_vert_c(cx,cy,cz, u_scale,v_scale, cc),
            sr_vert_c(dx,dy,dz, 0,v_scale, cd),
            tex, mvp);
    }
}

static void dng_draw_wall_ds(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                              float ax, float ay, float az,
                              float bx, float by, float bz,
                              float cx, float cy, float cz,
                              float dx, float dy, float dz,
                              const sr_indexed_texture *tex,
                              float nx, float ny, float nz) {
    dng_draw_wall(fb_ptr, mvp, ax,ay,az, bx,by,bz, cx,cy,cz, dx,dy,dz, tex, nx,ny,nz);
    dng_draw_wall(fb_ptr, mvp, bx,by,bz, ax,ay,az, dx,dy,dz, cx,cy,cz, tex, -nx,-ny,-nz);
}

static void dng_draw_hquad(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                            float ax, float ay, float az,
                            float bx, float by, float bz,
                            float cx, float cy, float cz,
                            float dx, float dy, float dz,
                            float u0, float v0, float u1, float v1,
                            const sr_indexed_texture *tex,
                            float nx, float ny, float nz) {
    if (dng_light_mode == 0) {
        sr_draw_quad_indexed_pixellit(fb_ptr,
            sr_vert_world(ax,ay,az, u0,v0, 0xFFFFFFFF, ax,ay,az, nx,ny,nz),
            sr_vert_world(bx,by,bz, u1,v0, 0xFFFFFFFF, bx,by,bz, nx,ny,nz),
            sr_vert_world(cx,cy,cz, u1,v1, 0xFFFFFFFF, cx,cy,cz, nx,ny,nz),
            sr_vert_world(dx,dy,dz, u0,v1, 0xFFFFFFFF, dx,dy,dz, nx,ny,nz),
            tex, mvp);
    } else {
        uint32_t ca = pal_intensity_color(dng_fog_vertex_intensity(ax,ay,az));
        uint32_t cb = pal_intensity_color(dng_fog_vertex_intensity(bx,by,bz));
        uint32_t cc = pal_intensity_color(dng_fog_vertex_intensity(cx,cy,cz));
        uint32_t cd = pal_intensity_color(dng_fog_vertex_intensity(dx,dy,dz));
        sr_draw_quad_indexed(fb_ptr,
            sr_vert_c(ax,ay,az, u0,v0, ca),
            sr_vert_c(bx,by,bz, u1,v0, cb),
            sr_vert_c(cx,cy,cz, u1,v1, cc),
            sr_vert_c(dx,dy,dz, u0,v1, cd),
            tex, mvp);
    }
}

/* ── Draw the dungeon scene ──────────────────────────────────────── */

static void draw_dungeon_scene(sr_framebuffer *fb_ptr, const sr_mat4 *vp) {
    sr_dungeon *d = dng_state.dungeon;
    dng_player *p = &dng_state.player;

    if (dng_light_mode == 0)
        sr_set_pixel_light_fn(dng_torch_light);

    float cam_x = p->x;
    float cam_y = p->y;
    float cam_z = p->z;
    float cam_angle = p->angle * 6.28318f;

    float ca_cos = cosf(cam_angle), ca_sin = sinf(cam_angle);
    sr_vec3 eye = { cam_x, cam_y, cam_z };
    sr_vec3 fwd = { ca_sin, 0, -ca_cos };
    sr_vec3 target = { eye.x + fwd.x, eye.y + fwd.y, eye.z + fwd.z };
    sr_vec3 up = { 0, 1, 0 };

    sr_mat4 view = sr_mat4_lookat(eye, target, up);
    sr_mat4 proj = sr_mat4_perspective(
        70.0f * 3.14159f / 180.0f,
        (float)FB_WIDTH / (float)FB_HEIGHT,
        0.05f, 40.0f
    );
    sr_mat4 mvp = sr_mat4_mul(proj, view);

    dng_build_visibility(p, d);

    float y_lo = -DNG_HALF_CELL;
    float y_hi = DNG_HALF_CELL;
    float P = DNG_PILLAR_PAD;

    int pgx = p->gx, pgy = p->gy;
    int gx0 = pgx - DNG_RENDER_R; if (gx0 < 1) gx0 = 1;
    int gx1 = pgx + DNG_RENDER_R; if (gx1 > d->w) gx1 = d->w;
    int gy0 = pgy - DNG_RENDER_R; if (gy0 < 1) gy0 = 1;
    int gy1 = pgy + DNG_RENDER_R; if (gy1 > d->h) gy1 = d->h;

    /* Render cells */
    for (int gy = gy0; gy <= gy1; gy++) {
        for (int gx = gx0; gx <= gx1; gx++) {
            if (!dng_vis[gy][gx]) continue;

            float x0 = (gx - 1) * DNG_CELL_SIZE;
            float x1 = gx * DNG_CELL_SIZE;
            float z0 = (gy - 1) * DNG_CELL_SIZE;
            float z1 = gy * DNG_CELL_SIZE;

            if (d->map[gy][gx] == 1) {
                /* Wall cell — draw faces toward open cells */
                if (gy < d->h && d->map[gy+1][gx] != 1 && dng_vis[gy+1][gx]) {
                    dng_draw_wall(fb_ptr, &mvp,
                        x0+P, y_hi, z1,  x1-P, y_hi, z1,
                        x1-P, y_lo, z1,  x0+P, y_lo, z1,
                        &itextures[ITEX_BRICK], 0, 0, 1);
                }
                if (gy > 1 && d->map[gy-1][gx] != 1 && dng_vis[gy-1][gx]) {
                    dng_draw_wall(fb_ptr, &mvp,
                        x1-P, y_hi, z0,  x0+P, y_hi, z0,
                        x0+P, y_lo, z0,  x1-P, y_lo, z0,
                        &itextures[ITEX_BRICK], 0, 0, -1);
                }
                if (gx < d->w && d->map[gy][gx+1] != 1 && dng_vis[gy][gx+1]) {
                    dng_draw_wall(fb_ptr, &mvp,
                        x1, y_hi, z1-P,  x1, y_hi, z0+P,
                        x1, y_lo, z0+P,  x1, y_lo, z1-P,
                        &itextures[ITEX_BRICK], 1, 0, 0);
                }
                if (gx > 1 && d->map[gy][gx-1] != 1 && dng_vis[gy][gx-1]) {
                    dng_draw_wall(fb_ptr, &mvp,
                        x0, y_hi, z0+P,  x0, y_hi, z1-P,
                        x0, y_lo, z1-P,  x0, y_lo, z0+P,
                        &itextures[ITEX_BRICK], -1, 0, 0);
                }
            } else {
                /* Open cell */
                bool is_up_stairs = (d->has_up && gx == d->stairs_gx && gy == d->stairs_gy);
                bool is_down_stairs = (d->has_down && gx == d->down_gx && gy == d->down_gy);

                if (is_up_stairs || is_down_stairs) {
                    int sdir = is_up_stairs ? d->stairs_dir : d->down_dir;
                    bool going_down = is_down_stairs;
                    float y_range = y_hi - y_lo;
                    float step_h = y_range / DNG_NUM_STEPS;
                    float step_d = DNG_CELL_SIZE / DNG_NUM_STEPS;

                    static const float riser_nx[4] = { 0, -1, 0,  1};
                    static const float riser_nz[4] = { 1,  0,-1,  0};

                    const sr_indexed_texture *stex = &itextures[ITEX_STONE];

                    for (int i = 0; i < DNG_NUM_STEPS; i++) {
                        float tread_y, riser_top, riser_bot;
                        float side_top, side_bot;
                        if (going_down) {
                            tread_y = y_lo - (i + 1) * step_h;
                            riser_top = y_lo - i * step_h;
                            riser_bot = tread_y;
                            side_top = y_lo;
                            side_bot = tread_y;
                        } else {
                            tread_y = y_lo + (i + 1) * step_h;
                            riser_top = tread_y;
                            riser_bot = y_lo + i * step_h;
                            side_top = tread_y;
                            side_bot = y_lo;
                        }

                        float sx0, sx1, sz0, sz1;
                        if (sdir == 0) { /* North */
                            sz0 = z1 - (i + 1) * step_d;
                            sz1 = z1 - i * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                x0,tread_y,sz0, x1,tread_y,sz0,
                                x1,tread_y,sz1, x0,tread_y,sz1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                x0,riser_top,sz1, x1,riser_top,sz1,
                                x1,riser_bot,sz1, x0,riser_bot,sz1,
                                stex, riser_nx[0],0,riser_nz[0]);
                            if (!going_down) {
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    x0,side_top,sz0, x0,side_top,sz1,
                                    x0,side_bot,sz1, x0,side_bot,sz0,
                                    stex, -1,0,0);
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    x1,side_top,sz1, x1,side_top,sz0,
                                    x1,side_bot,sz0, x1,side_bot,sz1,
                                    stex, 1,0,0);
                            }
                        } else if (sdir == 1) { /* East */
                            sx0 = x0 + i * step_d;
                            sx1 = x0 + (i + 1) * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                sx0,tread_y,z0, sx1,tread_y,z0,
                                sx1,tread_y,z1, sx0,tread_y,z1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                sx0,riser_top,z0, sx0,riser_top,z1,
                                sx0,riser_bot,z1, sx0,riser_bot,z0,
                                stex, riser_nx[1],0,riser_nz[1]);
                            if (!going_down) {
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    sx0,side_top,z0, sx1,side_top,z0,
                                    sx1,side_bot,z0, sx0,side_bot,z0,
                                    stex, 0,0,-1);
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    sx1,side_top,z1, sx0,side_top,z1,
                                    sx0,side_bot,z1, sx1,side_bot,z1,
                                    stex, 0,0,1);
                            }
                        } else if (sdir == 2) { /* South */
                            sz0 = z0 + i * step_d;
                            sz1 = z0 + (i + 1) * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                x0,tread_y,sz0, x1,tread_y,sz0,
                                x1,tread_y,sz1, x0,tread_y,sz1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                x1,riser_top,sz0, x0,riser_top,sz0,
                                x0,riser_bot,sz0, x1,riser_bot,sz0,
                                stex, riser_nx[2],0,riser_nz[2]);
                            if (!going_down) {
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    x0,side_top,sz1, x0,side_top,sz0,
                                    x0,side_bot,sz0, x0,side_bot,sz1,
                                    stex, -1,0,0);
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    x1,side_top,sz0, x1,side_top,sz1,
                                    x1,side_bot,sz1, x1,side_bot,sz0,
                                    stex, 1,0,0);
                            }
                        } else { /* West (3) */
                            sx0 = x1 - (i + 1) * step_d;
                            sx1 = x1 - i * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                sx0,tread_y,z0, sx1,tread_y,z0,
                                sx1,tread_y,z1, sx0,tread_y,z1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                sx1,riser_top,z1, sx1,riser_top,z0,
                                sx1,riser_bot,z0, sx1,riser_bot,z1,
                                stex, riser_nx[3],0,riser_nz[3]);
                            if (!going_down) {
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    sx1,side_top,z0, sx0,side_top,z0,
                                    sx0,side_bot,z0, sx1,side_bot,z0,
                                    stex, 0,0,-1);
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    sx0,side_top,z1, sx1,side_top,z1,
                                    sx1,side_bot,z1, sx0,side_bot,z1,
                                    stex, 0,0,1);
                            }
                        }
                    }

                    if (is_down_stairs) {
                        dng_draw_hquad(fb_ptr, &mvp,
                            x0,y_hi,z1, x1,y_hi,z1,
                            x1,y_hi,z0, x0,y_hi,z0,
                            0,1,1,0, &itextures[ITEX_WOOD], 0,-1,0);
                    }
                } else {
                    /* Normal floor + ceiling */
                    dng_draw_hquad(fb_ptr, &mvp,
                        x0,y_lo,z0, x1,y_lo,z0,
                        x1,y_lo,z1, x0,y_lo,z1,
                        0,0,1,1, &itextures[ITEX_TILE], 0,1,0);

                    dng_draw_hquad(fb_ptr, &mvp,
                        x0,y_hi,z1, x1,y_hi,z1,
                        x1,y_hi,z0, x0,y_hi,z0,
                        0,1,1,0, &itextures[ITEX_WOOD], 0,-1,0);
                }
            }
        }
    }

    /* Pillars at grid intersections */
    for (int vy = gy0; vy <= gy1 + 1; vy++) {
        for (int vx = gx0; vx <= gx1 + 1; vx++) {
            bool nw_open = vx > 1 && vy > 1 && d->map[vy-1][vx-1] != 1;
            bool ne_open = vx <= d->w && vy > 1 && d->map[vy-1][vx] != 1;
            bool sw_open = vx > 1 && vy <= d->h && d->map[vy][vx-1] != 1;
            bool se_open = vx <= d->w && vy <= d->h && d->map[vy][vx] != 1;

            bool has_open = nw_open || ne_open || sw_open || se_open;
            bool all_open = nw_open && ne_open && sw_open && se_open;

            if (has_open && !all_open) {
                bool visible = false;
                if (nw_open && vx-1 >= 1 && vy-1 >= 1 && dng_vis[vy-1][vx-1]) visible = true;
                if (ne_open && vx <= d->w && vy-1 >= 1 && dng_vis[vy-1][vx]) visible = true;
                if (sw_open && vx-1 >= 1 && vy <= d->h && dng_vis[vy][vx-1]) visible = true;
                if (se_open && vx <= d->w && vy <= d->h && dng_vis[vy][vx]) visible = true;
                if (!visible) continue;

                float wx = (vx - 1) * DNG_CELL_SIZE;
                float wz = (vy - 1) * DNG_CELL_SIZE;

                if (sw_open || se_open) {
                    float fx0 = sw_open ? wx - P : wx;
                    float fx1 = se_open ? wx + P : wx;
                    if (fx0 < fx1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            fx0, y_hi, wz+P,  fx1, y_hi, wz+P,
                            fx1, y_lo, wz+P,  fx0, y_lo, wz+P,
                            &itextures[ITEX_BRICK], 0, 0, 1);
                    }
                }
                if (nw_open || ne_open) {
                    float fx0 = nw_open ? wx - P : wx;
                    float fx1 = ne_open ? wx + P : wx;
                    if (fx0 < fx1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            fx1, y_hi, wz-P,  fx0, y_hi, wz-P,
                            fx0, y_lo, wz-P,  fx1, y_lo, wz-P,
                            &itextures[ITEX_BRICK], 0, 0, -1);
                    }
                }
                if (ne_open || se_open) {
                    float fz0 = ne_open ? wz - P : wz;
                    float fz1 = se_open ? wz + P : wz;
                    if (fz0 < fz1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            wx+P, y_hi, fz1,  wx+P, y_hi, fz0,
                            wx+P, y_lo, fz0,  wx+P, y_lo, fz1,
                            &itextures[ITEX_BRICK], 1, 0, 0);
                    }
                }
                if (nw_open || sw_open) {
                    float fz0 = nw_open ? wz - P : wz;
                    float fz1 = sw_open ? wz + P : wz;
                    if (fz0 < fz1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            wx-P, y_hi, fz0,  wx-P, y_hi, fz1,
                            wx-P, y_lo, fz1,  wx-P, y_lo, fz0,
                            &itextures[ITEX_BRICK], -1, 0, 0);
                    }
                }
            }
        }
    }
}

/* ── Minimap ─────────────────────────────────────────────────────── */

static void minimap_pixel(uint32_t *px, int rx, int ry, uint32_t col) {
    if (rx >= 0 && rx < FB_WIDTH && ry >= 0 && ry < FB_HEIGHT)
        px[ry * FB_WIDTH + rx] = col;
}

static void minimap_line(uint32_t *px, int x0, int y0, int x1, int y1, uint32_t col) {
    int ddx = x1 - x0; if (ddx < 0) ddx = -ddx;
    int ddy = y1 - y0; if (ddy < 0) ddy = -ddy;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = ddx - ddy;
    for (;;) {
        minimap_pixel(px, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -ddy) { err -= ddy; x0 += sx; }
        if (e2 <  ddx) { err += ddx; y0 += sy; }
    }
}

static void draw_dungeon_minimap(sr_framebuffer *fb_ptr) {
    sr_dungeon *d = dng_state.dungeon;
    dng_player *p = &dng_state.player;
    int scale = 2;
    int mx = FB_WIDTH - d->w * scale - 4;
    int my = 4;
    uint32_t *px = fb_ptr->color;

    for (int y = 1; y <= d->h; y++) {
        for (int x = 1; x <= d->w; x++) {
            if (d->map[y][x] == 1) continue;
            int px0 = mx + (x - 1) * scale;
            int py0 = my + (y - 1) * scale;
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++)
                    minimap_pixel(px, px0 + dx, py0 + dy, 0xFF444444);
        }
    }

    if (d->has_up) {
        int sx = d->stairs_gx, sy = d->stairs_gy;
        int px0 = mx + (sx - 1) * scale, py0 = my + (sy - 1) * scale;
        for (int dy = 0; dy < scale; dy++)
            for (int dx = 0; dx < scale; dx++)
                minimap_pixel(px, px0 + dx, py0 + dy, 0xFF00CC00);
    }
    if (d->has_down) {
        int sx = d->down_gx, sy = d->down_gy;
        int px0 = mx + (sx - 1) * scale, py0 = my + (sy - 1) * scale;
        for (int dy = 0; dy < scale; dy++)
            for (int dx = 0; dx < scale; dx++)
                minimap_pixel(px, px0 + dx, py0 + dy, 0xFF0000CC);
    }

    /* Player dot */
    float pcx = mx + (p->gx - 1) * scale + scale * 0.5f;
    float pcy = my + (p->gy - 1) * scale + scale * 0.5f;
    for (int dy = 0; dy < scale; dy++)
        for (int dx = 0; dx < scale; dx++)
            minimap_pixel(px, (int)pcx - scale/2 + dx, (int)pcy - scale/2 + dy, 0xFF00FFFF);

    /* View cone */
    float cone_len = 6.0f * scale;
    float face_angle = p->angle * 2.0f * 3.14159265f;
    float fwd_sx = sinf(face_angle);
    float fwd_sy = -cosf(face_angle);

    float half_fov = 0.52f;
    float cos_hf = cosf(half_fov), sin_hf = sinf(half_fov);

    float lx = fwd_sx * cos_hf - fwd_sy * sin_hf;
    float ly = fwd_sx * sin_hf + fwd_sy * cos_hf;
    float rx = fwd_sx * cos_hf + fwd_sy * sin_hf;
    float ry = -fwd_sx * sin_hf + fwd_sy * cos_hf;

    int lx1 = (int)(pcx + lx * cone_len);
    int ly1 = (int)(pcy + ly * cone_len);
    int rx1 = (int)(pcx + rx * cone_len);
    int ry1 = (int)(pcy + ry * cone_len);

    uint32_t cone_col = 0xFF00CCCC;
    minimap_line(px, (int)pcx, (int)pcy, lx1, ly1, cone_col);
    minimap_line(px, (int)pcx, (int)pcy, rx1, ry1, cone_col);
}

#endif /* SR_SCENE_DUNGEON_H */
