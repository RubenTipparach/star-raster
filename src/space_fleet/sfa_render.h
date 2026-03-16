/*  sfa_render.h — 3D ship models, starfield, boundary, projection, reticles.
 *  Header-only. Depends on sfa_types.h. */
#ifndef SFA_RENDER_H
#define SFA_RENDER_H

/* ── 3D Ship model ───────────────────────────────────────────────── */
/* Federation-style: saucer section (front), engineering hull (rear),
 * two nacelles on pylons. All geometry is 3D with proper Y height. */

static void sfa_draw_box(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                          float x0, float y0, float z0,
                          float x1, float y1, float z1,
                          uint32_t top_col, uint32_t side_col, uint32_t bottom_col) {
    /* Top (+Y) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x0, y1, z1, 0,0, top_col), sr_vert_c(x0, y1, z0, 0,1, top_col),
        sr_vert_c(x1, y1, z0, 1,1, top_col), sr_vert_c(x1, y1, z1, 1,0, top_col),
        NULL, mvp);
    /* Bottom (-Y) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x0, y0, z0, 0,0, bottom_col), sr_vert_c(x0, y0, z1, 0,1, bottom_col),
        sr_vert_c(x1, y0, z1, 1,1, bottom_col), sr_vert_c(x1, y0, z0, 1,0, bottom_col),
        NULL, mvp);
    /* Front (+Z) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x0, y0, z1, 0,0, side_col), sr_vert_c(x0, y1, z1, 0,1, side_col),
        sr_vert_c(x1, y1, z1, 1,1, side_col), sr_vert_c(x1, y0, z1, 1,0, side_col),
        NULL, mvp);
    /* Back (-Z) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x1, y0, z0, 0,0, side_col), sr_vert_c(x1, y1, z0, 0,1, side_col),
        sr_vert_c(x0, y1, z0, 1,1, side_col), sr_vert_c(x0, y0, z0, 1,0, side_col),
        NULL, mvp);
    /* Left (-X) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x0, y0, z0, 0,0, side_col), sr_vert_c(x0, y1, z0, 0,1, side_col),
        sr_vert_c(x0, y1, z1, 1,1, side_col), sr_vert_c(x0, y0, z1, 1,0, side_col),
        NULL, mvp);
    /* Right (+X) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x1, y0, z1, 0,0, side_col), sr_vert_c(x1, y1, z1, 0,1, side_col),
        sr_vert_c(x1, y1, z0, 1,1, side_col), sr_vert_c(x1, y0, z0, 1,0, side_col),
        NULL, mvp);
}

static void sfa_draw_ship(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                           sfa_ship *s) {
    float h = s->visual_heading;

    sr_mat4 model = sr_mat4_mul(
        sr_mat4_translate(s->x, 0.0f, s->z),
        sr_mat4_rotate_y(-h)
    );
    sr_mat4 mvp = sr_mat4_mul(*vp, model);

    /* Colors — use per-ship overrides if set */
    uint32_t hull_top   = s->color_hull   ? s->color_hull   : SFA_SHIP_COLOR;
    uint32_t nacelle_t  = s->color_accent ? s->color_accent : SFA_SHIP_ACCENT;
    /* Derive darker shades from hull color */
    uint8_t hr = (uint8_t)(hull_top & 0xFF);
    uint8_t hg = (uint8_t)((hull_top >> 8) & 0xFF);
    uint8_t hb = (uint8_t)((hull_top >> 16) & 0xFF);
    uint32_t hull_side = 0xFF000000 | ((uint32_t)(hb*3/4)<<16) | ((uint32_t)(hg*3/4)<<8) | (hr*3/4);
    uint32_t hull_bot  = 0xFF000000 | ((uint32_t)(hb/2)<<16) | ((uint32_t)(hg/2)<<8) | (hr/2);
    uint8_t nr = (uint8_t)(nacelle_t & 0xFF);
    uint8_t ng = (uint8_t)((nacelle_t >> 8) & 0xFF);
    uint8_t nb = (uint8_t)((nacelle_t >> 16) & 0xFF);
    uint32_t nacelle_s = 0xFF000000 | ((uint32_t)(nb*3/4)<<16) | ((uint32_t)(ng*3/4)<<8) | (nr*3/4);
    uint32_t nacelle_b = 0xFF000000 | ((uint32_t)(nb/2)<<16) | ((uint32_t)(ng/2)<<8) | (nr/2);
    uint32_t pylon_col  = 0xFF887755;
    uint32_t bridge_col = 0xFFFFDDAA;
    uint32_t deflector  = 0xFFFFAA33;  /* blue-ish glow (ABGR) */

    /* ── Saucer section (front disc approximated as octagonal prism) ── */
    {
        float sr = 0.9f;    /* saucer radius */
        float sh = 0.12f;   /* saucer half-height */
        float sy = 0.15f;   /* saucer Y center */
        float sz = 0.6f;    /* saucer Z center (forward) */
        int n = 8;
        float angles[9]; /* n+1 for closing */
        for (int i = 0; i <= n; i++)
            angles[i] = SFA_TWO_PI * (float)i / (float)n;

        for (int i = 0; i < n; i++) {
            float c0 = cosf(angles[i]),   s0 = sinf(angles[i]);
            float c1 = cosf(angles[i+1]), s1 = sinf(angles[i+1]);
            float x0 = s0 * sr, z0 = c0 * sr + sz;
            float x1 = s1 * sr, z1 = c1 * sr + sz;

            /* Top face wedge */
            sr_draw_triangle(fb_ptr,
                sr_vert_c(0, sy + sh, sz, 0.5f, 0.5f, hull_top),
                sr_vert_c(x1, sy + sh, z1, 1, 0, hull_top),
                sr_vert_c(x0, sy + sh, z0, 0, 0, hull_top),
                NULL, &mvp);
            /* Bottom face wedge */
            sr_draw_triangle(fb_ptr,
                sr_vert_c(0, sy - sh, sz, 0.5f, 0.5f, hull_bot),
                sr_vert_c(x0, sy - sh, z0, 0, 0, hull_bot),
                sr_vert_c(x1, sy - sh, z1, 1, 0, hull_bot),
                NULL, &mvp);
            /* Side rim */
            sr_draw_quad(fb_ptr,
                sr_vert_c(x0, sy - sh, z0, 0, 0, hull_side),
                sr_vert_c(x1, sy - sh, z1, 1, 0, hull_side),
                sr_vert_c(x1, sy + sh, z1, 1, 1, hull_side),
                sr_vert_c(x0, sy + sh, z0, 0, 1, hull_side),
                NULL, &mvp);
        }

        /* Bridge dome (small raised box on top of saucer) */
        sfa_draw_box(fb_ptr, &mvp,
                     -0.15f, sy + sh, sz - 0.15f,
                      0.15f, sy + sh + 0.12f, sz + 0.15f,
                     bridge_col, bridge_col, hull_top);
    }

    /* ── Engineering hull (rear section — elongated box) ── */
    {
        float ew = 0.25f;  /* half-width */
        float eh = 0.2f;   /* half-height */
        float ey = 0.0f;   /* Y center */
        float ez0 = -1.4f; /* aft end */
        float ez1 = 0.3f;  /* connects to saucer */

        sfa_draw_box(fb_ptr, &mvp,
                     -ew, ey - eh, ez0,
                      ew, ey + eh, ez1,
                     hull_top, hull_side, hull_bot);

        /* Deflector dish (front face of engineering hull) */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-ew*0.6f, ey - eh*0.6f, ez1 + 0.01f, 0,0, deflector),
            sr_vert_c(-ew*0.6f, ey + eh*0.6f, ez1 + 0.01f, 0,1, deflector),
            sr_vert_c( ew*0.6f, ey + eh*0.6f, ez1 + 0.01f, 1,1, deflector),
            sr_vert_c( ew*0.6f, ey - eh*0.6f, ez1 + 0.01f, 1,0, deflector),
            NULL, &mvp);
    }

    /* ── Nacelle pylons (diagonal struts from engineering to nacelles) ── */
    {
        float pw = 0.06f;  /* pylon thickness */
        /* Left pylon */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-0.25f, 0.0f,  -0.4f, 0,0, pylon_col),
            sr_vert_c(-0.25f, 0.0f,  -0.7f, 0,1, pylon_col),
            sr_vert_c(-0.85f, 0.35f, -0.7f, 1,1, pylon_col),
            sr_vert_c(-0.85f, 0.35f, -0.4f, 1,0, pylon_col),
            NULL, &mvp);
        /* Pylon thickness (top face) */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-0.25f,      pw, -0.4f, 0,0, pylon_col),
            sr_vert_c(-0.85f, 0.35f+pw, -0.4f, 0,1, pylon_col),
            sr_vert_c(-0.85f, 0.35f+pw, -0.7f, 1,1, pylon_col),
            sr_vert_c(-0.25f,      pw, -0.7f, 1,0, pylon_col),
            NULL, &mvp);

        /* Right pylon (mirror) */
        sr_draw_quad(fb_ptr,
            sr_vert_c(0.25f, 0.0f,  -0.7f, 0,0, pylon_col),
            sr_vert_c(0.25f, 0.0f,  -0.4f, 0,1, pylon_col),
            sr_vert_c(0.85f, 0.35f, -0.4f, 1,1, pylon_col),
            sr_vert_c(0.85f, 0.35f, -0.7f, 1,0, pylon_col),
            NULL, &mvp);
        sr_draw_quad(fb_ptr,
            sr_vert_c(0.25f,      pw, -0.7f, 0,0, pylon_col),
            sr_vert_c(0.85f, 0.35f+pw, -0.7f, 0,1, pylon_col),
            sr_vert_c(0.85f, 0.35f+pw, -0.4f, 1,1, pylon_col),
            sr_vert_c(0.25f,      pw, -0.4f, 1,0, pylon_col),
            NULL, &mvp);
    }

    /* ── Nacelles (elongated boxes, raised on pylons) ── */
    {
        float nw = 0.12f;  /* nacelle half-width */
        float nh = 0.1f;   /* nacelle half-height */
        float ny = 0.4f;   /* nacelle Y center */
        float nz0 = -1.3f; /* aft */
        float nz1 = 0.0f;  /* fore */
        float nx = 0.85f;  /* X offset from center */

        /* Left nacelle */
        sfa_draw_box(fb_ptr, &mvp,
                     -nx - nw, ny - nh, nz0,
                     -nx + nw, ny + nh, nz1,
                     nacelle_t, nacelle_s, nacelle_b);

        /* Right nacelle */
        sfa_draw_box(fb_ptr, &mvp,
                      nx - nw, ny - nh, nz0,
                      nx + nw, ny + nh, nz1,
                     nacelle_t, nacelle_s, nacelle_b);

        /* Bussard collectors (front caps — red glow) */
        uint32_t bussard = 0xFF2222FF;  /* red in ABGR */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-nx-nw, ny-nh, nz1+0.01f, 0,0, bussard),
            sr_vert_c(-nx-nw, ny+nh, nz1+0.01f, 0,1, bussard),
            sr_vert_c(-nx+nw, ny+nh, nz1+0.01f, 1,1, bussard),
            sr_vert_c(-nx+nw, ny-nh, nz1+0.01f, 1,0, bussard),
            NULL, &mvp);
        sr_draw_quad(fb_ptr,
            sr_vert_c(nx-nw, ny-nh, nz1+0.01f, 0,0, bussard),
            sr_vert_c(nx-nw, ny+nh, nz1+0.01f, 0,1, bussard),
            sr_vert_c(nx+nw, ny+nh, nz1+0.01f, 1,1, bussard),
            sr_vert_c(nx+nw, ny-nh, nz1+0.01f, 1,0, bussard),
            NULL, &mvp);
    }

    /* ── Engine glow (exhaust from nacelle rears) ── */
    if (s->current_speed > 0.1f) {
        float speed_frac = s->current_speed / sfa_speed_values[SFA_NUM_SPEEDS - 1];
        float glow_len = 0.4f + 0.9f * speed_frac;
        float pulse = 0.7f + 0.3f * sinf(sfa.time * 12.0f);
        uint8_t gr = (uint8_t)(255.0f * pulse);
        uint8_t gg = (uint8_t)(100.0f * pulse);
        uint32_t glow_col = 0xFF000000 | (uint32_t)(0x22) << 16 | (uint32_t)gg << 8 | gr;

        float nx = 0.85f;
        float ny = 0.4f;
        float nz0 = -1.3f;

        /* Left nacelle exhaust */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(-nx, ny + 0.05f, nz0,             0.5f, 0, glow_col),
            sr_vert_c(-nx - 0.06f, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            sr_vert_c(-nx + 0.06f, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            NULL, &mvp);
        sr_draw_triangle(fb_ptr,
            sr_vert_c(-nx, ny - 0.05f, nz0,             0.5f, 0, glow_col),
            sr_vert_c(-nx + 0.06f, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            sr_vert_c(-nx - 0.06f, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            NULL, &mvp);

        /* Right nacelle exhaust */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(nx, ny + 0.05f, nz0,             0.5f, 0, glow_col),
            sr_vert_c(nx + 0.06f, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            sr_vert_c(nx - 0.06f, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            NULL, &mvp);
        sr_draw_triangle(fb_ptr,
            sr_vert_c(nx, ny - 0.05f, nz0,             0.5f, 0, glow_col),
            sr_vert_c(nx - 0.06f, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            sr_vert_c(nx + 0.06f, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            NULL, &mvp);

        /* Main impulse engine exhaust (rear of engineering hull) */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(0, 0.05f, -1.4f,              0.5f, 0, glow_col),
            sr_vert_c(-0.12f, 0, -1.4f - glow_len*0.6f, 0, 1, 0xFF000000),
            sr_vert_c( 0.12f, 0, -1.4f - glow_len*0.6f, 1, 1, 0xFF000000),
            NULL, &mvp);
    }
}

/* ── 3D Starfield (pixel-based with palette shading) ─────────────── */

static void sfa_draw_starfield(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                 float cam_x, float cam_z) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    float spacing = SFA_GRID_SPACING;
    float view_range = 50.0f;
    float y_range = 30.0f;
    float y_center = 5.0f;

    float x_start = floorf((cam_x - view_range) / spacing) * spacing;
    float z_start = floorf((cam_z - view_range) / spacing) * spacing;
    float y_start = floorf((y_center - y_range) / spacing) * spacing;

    /* Star base colors (ABGR): 3 white, 1 blue */
    static const uint32_t star_colors[4] = {
        0xFFFFFFFF,  /* pure white */
        0xFFFFFFFF,  /* pure white */
        0xFFFFFFFF,  /* pure white */
        0xFFFFCC88,  /* light blue (ABGR: R=0x88, G=0xCC, B=0xFF) */
    };

    for (float gx = x_start; gx < cam_x + view_range; gx += spacing) {
        for (float gz = z_start; gz < cam_z + view_range; gz += spacing) {
            for (float gy = y_start; gy < y_center + y_range; gy += spacing) {
                int ix = (int)floorf(gx / spacing);
                int iy = (int)floorf(gy / spacing);
                int iz = (int)floorf(gz / spacing);
                uint32_t seed = (uint32_t)(ix * 73856093u ^ iy * 83492791u ^ iz * 19349663u);

                /* Skip ~50% of cells (with doubled spacing = ~25% total density) */
                if ((seed & 3) == 0) continue;

                /* 1 star per cell */
                for (int si = 0; si < 1; si++) {
                    seed = seed * 1103515245u + 12345u + (uint32_t)si * 7u;
                    float ox = (float)((seed >> 4) & 0xFF) / 255.0f * spacing;
                    seed = seed * 1103515245u + 12345u;
                    float oy = (float)((seed >> 4) & 0xFF) / 255.0f * spacing;
                    seed = seed * 1103515245u + 12345u;
                    float oz = (float)((seed >> 4) & 0xFF) / 255.0f * spacing;
                    seed = seed * 1103515245u + 12345u;
                    float brightness = 0.12f + (float)((seed >> 4) & 0xFF) / 255.0f * 0.6f;

                    float star_x = gx + ox;
                    float star_y = gy + oy;
                    float star_z = gz + oz;

                    /* Distance fade */
                    float ddx = star_x - cam_x;
                    float ddz = star_z - cam_z;
                    float dist2 = ddx * ddx + ddz * ddz;
                    if (dist2 > view_range * view_range) continue;
                    float dist_fade = 1.0f - dist2 / (view_range * view_range);
                    brightness *= dist_fade;
                    if (brightness < 0.02f) continue;

                    /* Project to screen */
                    sr_vec4 clip = sr_mat4_mul_v4(*vp, sr_v4(star_x, star_y, star_z, 1.0f));
                    if (clip.w < 0.1f) continue;
                    float inv_w = 1.0f / clip.w;
                    float ndc_x = clip.x * inv_w;
                    float ndc_y = clip.y * inv_w;

                    int sx = (int)((ndc_x * 0.5f + 0.5f) * W);
                    int sy = (int)((1.0f - (ndc_y * 0.5f + 0.5f)) * H);

                    if (sx < 0 || sx >= W || sy < 0 || sy >= H) continue;

                    /* Pick tint and scale by brightness */
                    seed = seed * 1103515245u + 12345u;
                    uint32_t base = star_colors[(seed >> 8) % 4];
                    uint8_t br = (uint8_t)(((base      ) & 0xFF) * brightness);
                    uint8_t bg = (uint8_t)(((base >>  8) & 0xFF) * brightness);
                    uint8_t bb = (uint8_t)(((base >> 16) & 0xFF) * brightness);
                    uint32_t col = 0xFF000000 | ((uint32_t)bb << 16) | ((uint32_t)bg << 8) | br;

                    /* Close stars (< 40% range) = 2x2, far = 1x1 */
                    float dist_norm = dist2 / (view_range * view_range);
                    if (dist_norm < 0.16f && brightness > 0.25f) {
                        px[sy * W + sx] = col;
                        if (sx + 1 < W) px[sy * W + sx + 1] = col;
                        if (sy + 1 < H) px[(sy + 1) * W + sx] = col;
                        if (sx + 1 < W && sy + 1 < H) px[(sy + 1) * W + sx + 1] = col;
                    } else {
                        px[sy * W + sx] = col;
                    }
                }
            }
        }
    }
}

/* ── Arena boundary markers ──────────────────────────────────────── */

static void sfa_draw_arena_boundary(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                       float px, float pz) {
    float s = SFA_ARENA_SIZE;
    float y = -0.05f;
    float vis_dist = 40.0f;  /* distance at which border starts appearing */

    /* Only draw edges the player is near — compute distance to each edge */
    struct { float x0,z0,x1,z1; float player_dist; } edges[4] = {
        { -s, -s, -s,  s, px - (-s) },   /* left   (player dist = px + s) */
        {  s, -s,  s,  s, s - px     },   /* right  (player dist = s - px) */
        { -s, -s,  s, -s, pz - (-s) },   /* bottom (player dist = pz + s) */
        { -s,  s,  s,  s, s - pz     },   /* top    (player dist = s - pz) */
    };

    for (int i = 0; i < 4; i++) {
        if (edges[i].player_dist > vis_dist) continue;

        /* Brightness increases as player gets closer */
        float t = 1.0f - (edges[i].player_dist / vis_dist);
        if (t < 0) t = 0;
        uint8_t r_val = (uint8_t)(255.0f * t);
        uint8_t g_val = (uint8_t)(30.0f * t);
        uint32_t col = 0xFF000000 | (g_val << 8) | r_val;  /* ABGR red */

        /* Thicker line when closer */
        float w = 0.15f + t * 0.5f;

        float x0 = edges[i].x0, z0 = edges[i].z0;
        float x1 = edges[i].x1, z1 = edges[i].z1;

        if (x0 == x1) {
            /* Vertical edge (left or right) */
            float dx = (x0 < 0) ? w : -w;
            sr_draw_quad(fb_ptr,
                sr_vert_c(x0,    y, z0, 0,0, col), sr_vert_c(x0,    y, z1, 0,1, col),
                sr_vert_c(x0+dx, y, z1, 1,1, col), sr_vert_c(x0+dx, y, z0, 1,0, col),
                NULL, vp);
        } else {
            /* Horizontal edge (top or bottom) */
            float dz = (z0 < 0) ? w : -w;
            sr_draw_quad(fb_ptr,
                sr_vert_c(x0, y, z0,    0,0, col), sr_vert_c(x0, y, z0+dz, 0,1, col),
                sr_vert_c(x1, y, z0+dz, 1,1, col), sr_vert_c(x1, y, z0,    1,0, col),
                NULL, vp);
        }
    }
}

/* ── Target drawing & projection ─────────────────────────────────── */

/* Draw Klingon Bird of Prey — swept wings, central command pod, neck */
static void sfa_draw_target_ship(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                   float tx, float tz, float heading) {
    uint32_t hull_t = sfa_pal_abgr(30); /* 239063 dark green */
    uint32_t hull_s = sfa_pal_abgr(29); /* 165a4c darker green */
    uint32_t hull_b = sfa_pal_abgr(35); /* 374e4a darkest */
    uint32_t wing_t = sfa_pal_abgr(36); /* 547e64 olive green */
    uint32_t wing_s = sfa_pal_abgr(35); /* 374e4a */
    uint32_t wing_b = sfa_pal_abgr(34); /* 313638 near-black */
    uint32_t head_t = sfa_pal_abgr(31); /* 1ebc73 bright green */
    uint32_t head_s = sfa_pal_abgr(30);
    uint32_t gun_col = sfa_pal_abgr(15); /* e83b3b red — disruptor */

    sr_mat4 model = sr_mat4_mul(
        sr_mat4_translate(tx, 0.0f, tz),
        sr_mat4_rotate_y(-heading)
    );
    sr_mat4 mvp = sr_mat4_mul(*vp, model);

    /* Central body (aft section) */
    sfa_draw_box(fb_ptr, &mvp,
                 -0.3f, -0.1f, -0.9f,
                  0.3f,  0.15f, 0.0f,
                 hull_t, hull_s, hull_b);

    /* Neck (connecting body to head) */
    sfa_draw_box(fb_ptr, &mvp,
                 -0.1f, -0.05f, 0.0f,
                  0.1f,  0.08f, 0.7f,
                 hull_t, hull_s, hull_b);

    /* Command pod (head) */
    sfa_draw_box(fb_ptr, &mvp,
                 -0.2f, -0.08f, 0.7f,
                  0.2f,  0.12f, 1.1f,
                 head_t, head_s, hull_b);

    /* Disruptor cannon (front of head) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.08f, -0.02f, 1.11f, 0,0, gun_col),
        sr_vert_c(-0.08f,  0.06f, 1.11f, 0,1, gun_col),
        sr_vert_c( 0.08f,  0.06f, 1.11f, 1,1, gun_col),
        sr_vert_c( 0.08f, -0.02f, 1.11f, 1,0, gun_col),
        NULL, &mvp);

    /* Left swept wing — angled down and forward */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.3f,  0.05f, -0.6f, 0,0, wing_t),
        sr_vert_c(-0.3f,  0.05f, -0.1f, 0,1, wing_t),
        sr_vert_c(-1.4f, -0.15f,  0.4f, 1,1, wing_t),
        sr_vert_c(-1.4f, -0.15f, -0.2f, 1,0, wing_t),
        NULL, &mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.3f, -0.02f, -0.6f, 0,0, wing_b),
        sr_vert_c(-1.4f, -0.18f, -0.2f, 1,0, wing_b),
        sr_vert_c(-1.4f, -0.18f,  0.4f, 1,1, wing_b),
        sr_vert_c(-0.3f, -0.02f, -0.1f, 0,1, wing_b),
        NULL, &mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.3f, -0.02f, -0.1f, 0,0, wing_s),
        sr_vert_c(-1.4f, -0.18f,  0.4f, 1,0, wing_s),
        sr_vert_c(-1.4f, -0.15f,  0.4f, 1,1, wing_s),
        sr_vert_c(-0.3f,  0.05f, -0.1f, 0,1, wing_s),
        NULL, &mvp);
    sfa_draw_box(fb_ptr, &mvp,
                 -1.5f, -0.18f, 0.1f,
                 -1.35f,-0.1f,  0.5f,
                 gun_col, gun_col, gun_col);

    /* Right swept wing (mirror) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(0.3f,  0.05f, -0.1f, 0,0, wing_t),
        sr_vert_c(0.3f,  0.05f, -0.6f, 0,1, wing_t),
        sr_vert_c(1.4f, -0.15f, -0.2f, 1,1, wing_t),
        sr_vert_c(1.4f, -0.15f,  0.4f, 1,0, wing_t),
        NULL, &mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(0.3f, -0.02f, -0.1f, 0,0, wing_b),
        sr_vert_c(0.3f, -0.02f, -0.6f, 0,1, wing_b),
        sr_vert_c(1.4f, -0.18f, -0.2f, 1,1, wing_b),
        sr_vert_c(1.4f, -0.18f,  0.4f, 1,0, wing_b),
        NULL, &mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(0.3f,  0.05f, -0.1f, 0,0, wing_s),
        sr_vert_c(0.3f, -0.02f, -0.1f, 0,1, wing_s),
        sr_vert_c(1.4f, -0.18f,  0.4f, 1,1, wing_s),
        sr_vert_c(1.4f, -0.15f,  0.4f, 1,0, wing_s),
        NULL, &mvp);
    sfa_draw_box(fb_ptr, &mvp,
                 1.35f, -0.18f, 0.1f,
                 1.5f,  -0.1f,  0.5f,
                 gun_col, gun_col, gun_col);
}

/* ── 4x4 matrix inverse (cofactor expansion) ────────────────────── */

static bool sfa_mat4_invert(const sr_mat4 *m, sr_mat4 *out) {
    const float *s = &m->m[0][0];
    float inv[16];

    inv[0]  =  s[5]*s[10]*s[15] - s[5]*s[11]*s[14] - s[9]*s[6]*s[15]
             + s[9]*s[7]*s[14]  + s[13]*s[6]*s[11]  - s[13]*s[7]*s[10];
    inv[4]  = -s[4]*s[10]*s[15] + s[4]*s[11]*s[14]  + s[8]*s[6]*s[15]
             - s[8]*s[7]*s[14]  - s[12]*s[6]*s[11]  + s[12]*s[7]*s[10];
    inv[8]  =  s[4]*s[9]*s[15]  - s[4]*s[11]*s[13]  - s[8]*s[5]*s[15]
             + s[8]*s[7]*s[13]  + s[12]*s[5]*s[11]  - s[12]*s[7]*s[9];
    inv[12] = -s[4]*s[9]*s[14]  + s[4]*s[10]*s[13]  + s[8]*s[5]*s[14]
             - s[8]*s[6]*s[13]  - s[12]*s[5]*s[10]  + s[12]*s[6]*s[9];

    float det = s[0]*inv[0] + s[1]*inv[4] + s[2]*inv[8] + s[3]*inv[12];
    if (det > -1e-8f && det < 1e-8f) return false;

    inv[1]  = -s[1]*s[10]*s[15] + s[1]*s[11]*s[14]  + s[9]*s[2]*s[15]
             - s[9]*s[3]*s[14]  - s[13]*s[2]*s[11]  + s[13]*s[3]*s[10];
    inv[5]  =  s[0]*s[10]*s[15] - s[0]*s[11]*s[14]  - s[8]*s[2]*s[15]
             + s[8]*s[3]*s[14]  + s[12]*s[2]*s[11]  - s[12]*s[3]*s[10];
    inv[9]  = -s[0]*s[9]*s[15]  + s[0]*s[11]*s[13]  + s[8]*s[1]*s[15]
             - s[8]*s[3]*s[13]  - s[12]*s[1]*s[11]  + s[12]*s[3]*s[9];
    inv[13] =  s[0]*s[9]*s[14]  - s[0]*s[10]*s[13]  - s[8]*s[1]*s[14]
             + s[8]*s[2]*s[13]  + s[12]*s[1]*s[10]  - s[12]*s[2]*s[9];

    inv[2]  =  s[1]*s[6]*s[15]  - s[1]*s[7]*s[14]   - s[5]*s[2]*s[15]
             + s[5]*s[3]*s[14]  + s[13]*s[2]*s[7]    - s[13]*s[3]*s[6];
    inv[6]  = -s[0]*s[6]*s[15]  + s[0]*s[7]*s[14]    + s[4]*s[2]*s[15]
             - s[4]*s[3]*s[14]  - s[12]*s[2]*s[7]    + s[12]*s[3]*s[6];
    inv[10] =  s[0]*s[5]*s[15]  - s[0]*s[7]*s[13]    - s[4]*s[1]*s[15]
             + s[4]*s[3]*s[13]  + s[12]*s[1]*s[7]    - s[12]*s[3]*s[5];
    inv[14] = -s[0]*s[5]*s[14]  + s[0]*s[6]*s[13]    + s[4]*s[1]*s[14]
             - s[4]*s[2]*s[13]  - s[12]*s[1]*s[6]    + s[12]*s[2]*s[5];

    inv[3]  = -s[1]*s[6]*s[11]  + s[1]*s[7]*s[10]    + s[5]*s[2]*s[11]
             - s[5]*s[3]*s[10]  - s[9]*s[2]*s[7]     + s[9]*s[3]*s[6];
    inv[7]  =  s[0]*s[6]*s[11]  - s[0]*s[7]*s[10]    - s[4]*s[2]*s[11]
             + s[4]*s[3]*s[10]  + s[8]*s[2]*s[7]     - s[8]*s[3]*s[6];
    inv[11] = -s[0]*s[5]*s[11]  + s[0]*s[7]*s[9]     + s[4]*s[1]*s[11]
             - s[4]*s[3]*s[9]   - s[8]*s[1]*s[7]     + s[8]*s[3]*s[5];
    inv[15] =  s[0]*s[5]*s[10]  - s[0]*s[6]*s[9]     - s[4]*s[1]*s[10]
             + s[4]*s[2]*s[9]   + s[8]*s[1]*s[6]     - s[8]*s[2]*s[5];

    float inv_det = 1.0f / det;
    float *o = &out->m[0][0];
    for (int i = 0; i < 16; i++) o[i] = inv[i] * inv_det;
    return true;
}

/* Unproject a framebuffer pixel to a ray, intersect with Y=0 plane.
   Returns true if hit, writes world XZ to out_x/out_z. */
static bool sfa_screen_to_ground(float fb_x, float fb_y, int W, int H,
                                  const sr_mat4 *inv_vp, sr_vec3 eye,
                                  float *out_x, float *out_z) {
    /* NDC coords from framebuffer pixel */
    float ndc_x = (fb_x / (float)W) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (fb_y / (float)H) * 2.0f;

    /* Unproject near and far points */
    sr_vec4 near_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
    sr_vec4 far_clip  = { ndc_x, ndc_y,  1.0f, 1.0f };
    sr_vec4 near_w = sr_mat4_mul_v4(*inv_vp, near_clip);
    sr_vec4 far_w  = sr_mat4_mul_v4(*inv_vp, far_clip);

    if (near_w.w == 0 || far_w.w == 0) return false;
    float nw = 1.0f / near_w.w, fw = 1.0f / far_w.w;
    float nx = near_w.x * nw, ny = near_w.y * nw, nz = near_w.z * nw;
    float ffx = far_w.x * fw,  fy = far_w.y * fw,  fz = far_w.z * fw;

    /* Ray direction */
    float dx = ffx - nx, dy = fy - ny, dz = fz - nz;

    /* Intersect with Y=0 plane */
    if (dy > -1e-6f && dy < 1e-6f) return false; /* parallel to plane */
    float t = -ny / dy;
    if (t < 0) return false; /* behind camera */

    *out_x = nx + dx * t;
    *out_z = nz + dz * t;
    return true;
}

/* Project a world point to framebuffer coords. Returns false if behind camera. */
static bool sfa_project_to_screen(const sr_mat4 *vp, float wx, float wy, float wz,
                                    int W, int H, int *out_sx, int *out_sy, float *out_w) {
    sr_vec4 clip = sr_mat4_mul_v4(*vp, sr_v4(wx, wy, wz, 1.0f));
    *out_w = clip.w;
    if (clip.w < 0.1f) {
        /* Behind camera — compute direction for edge indicator */
        *out_sx = (clip.x < 0) ? W - 1 : 0;
        *out_sy = (clip.y > 0) ? 0 : H - 1;
        return false;
    }
    float inv_w = 1.0f / clip.w;
    float ndc_x = clip.x * inv_w;
    float ndc_y = clip.y * inv_w;
    *out_sx = (int)((ndc_x * 0.5f + 0.5f) * W);
    *out_sy = (int)((1.0f - (ndc_y * 0.5f + 0.5f)) * H);
    return true;
}

/* Compute screen-space bracket half-size by projecting ship bounding box corners.
   Returns the max pixel offset from center in X or Y. */
static int sfa_ship_screen_extent(const sr_mat4 *vp, float ship_x, float ship_z,
                                    float heading, int W, int H) {
    /* Target ship local-space bounding box (from vertex data) */
    static const float bbox_pts[][3] = {
        {-1.5f, -0.18f, -0.9f}, { 1.5f, -0.18f, -0.9f},
        {-1.5f,  0.15f, -0.9f}, { 1.5f,  0.15f, -0.9f},
        {-1.5f, -0.18f,  1.11f},{ 1.5f, -0.18f,  1.11f},
        {-1.5f,  0.15f,  1.11f},{ 1.5f,  0.15f,  1.11f},
    };

    float ch = cosf(-heading), sh = sinf(-heading);
    int cx, cy;
    float cw;
    if (!sfa_project_to_screen(vp, ship_x, 0.0f, ship_z, W, H, &cx, &cy, &cw))
        return 12;

    int max_off = 0;
    for (int i = 0; i < 8; i++) {
        /* Rotate local point by heading, translate to world */
        float lx = bbox_pts[i][0], ly = bbox_pts[i][1], lz = bbox_pts[i][2];
        float wx = ship_x + lx * ch - lz * sh;
        float wz = ship_z + lx * sh + lz * ch;
        float wy = ly;

        int sx, sy; float sw;
        if (!sfa_project_to_screen(vp, wx, wy, wz, W, H, &sx, &sy, &sw))
            continue;
        int dx = sx - cx, dy = sy - cy;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (dx > max_off) max_off = dx;
        if (dy > max_off) max_off = dy;
    }

    /* Add small padding, clamp */
    max_off += 3;
    if (max_off < 8) max_off = 8;
    if (max_off > 80) max_off = 80;
    return max_off;
}

/* Draw targeting reticle brackets around a screen point */
static void sfa_draw_reticle(uint32_t *px, int W, int H,
                               int cx, int cy, bool selected) {
    uint32_t col = selected ? sfa_pal_abgr(15) : sfa_pal_abgr(12); /* red / orange */
    int r = selected ? 10 : 8;  /* bracket size */
    int t = 3;                  /* bracket arm length */

    /* Four corner brackets */
    for (int i = 0; i < t; i++) {
        /* Top-left */
        int x, y;
        x = cx - r + i; y = cy - r;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        x = cx - r; y = cy - r + i;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        /* Top-right */
        x = cx + r - i; y = cy - r;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        x = cx + r; y = cy - r + i;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        /* Bottom-left */
        x = cx - r + i; y = cy + r;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        x = cx - r; y = cy + r - i;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        /* Bottom-right */
        x = cx + r - i; y = cy + r;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        x = cx + r; y = cy + r - i;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
    }
}

/* Draw off-screen target indicator clamped to screen edge */
static void sfa_draw_offscreen_indicator(uint32_t *px, int W, int H,
                                           int tx, int ty, bool selected) {
    /* Clamp to screen edge with margin */
    int margin = 12;
    int cx = tx, cy = ty;
    if (cx < margin) cx = margin;
    if (cx >= W - margin) cx = W - margin - 1;
    if (cy < margin + 14) cy = margin + 14; /* avoid HUD top bar */
    if (cy >= H - margin) cy = H - margin - 1;

    uint32_t col = selected ? sfa_pal_abgr(15) : sfa_pal_abgr(12);

    /* Draw small arrow pointing toward target */
    float dx = (float)(tx - cx);
    float dy = (float)(ty - cy);
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) len = 1.0f;
    dx /= len; dy /= len;

    /* Arrow tip */
    int ax = cx + (int)(dx * 5);
    int ay = cy + (int)(dy * 5);
    /* Draw a 3x3 filled square at the indicator position */
    for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
            int px2 = cx + ox, py2 = cy + oy;
            if (px2 >= 0 && px2 < W && py2 >= 0 && py2 < H)
                px[py2 * W + px2] = col;
        }
    /* Arrow line */
    if (ax >= 0 && ax < W && ay >= 0 && ay < H)
        px[ay * W + ax] = col;
    int mx = cx + (int)(dx * 3);
    int my = cy + (int)(dy * 3);
    if (mx >= 0 && mx < W && my >= 0 && my < H)
        px[my * W + mx] = col;
}

#endif /* SFA_RENDER_H */
