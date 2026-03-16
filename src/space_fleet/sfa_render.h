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

/* ── Parameterized Federation ship model ─────────────────────────── */
/* Builds a Federation-style ship with configurable proportions.     */
typedef struct {
    float saucer_r;       /* saucer disc radius */
    float saucer_h;       /* saucer half-height */
    float saucer_y;       /* saucer Y center */
    float saucer_z;       /* saucer Z center (forward) */
    float eng_hw;         /* engineering hull half-width */
    float eng_hh;         /* engineering hull half-height */
    float eng_z0;         /* engineering hull aft Z */
    float eng_z1;         /* engineering hull fore Z */
    float nacelle_x;      /* nacelle X offset from center */
    float nacelle_y;      /* nacelle Y center */
    float nacelle_hw;     /* nacelle half-width */
    float nacelle_hh;     /* nacelle half-height */
    float nacelle_z0;     /* nacelle aft Z */
    float nacelle_z1;     /* nacelle fore Z */
    float pylon_top_y;    /* pylon top Y (connects to nacelle) */
    float bridge_r;       /* bridge dome half-extent */
    float bridge_h;       /* bridge dome height */
    bool  has_secondary;  /* battlecruiser: extra secondary hull */
} fed_ship_params;

static const fed_ship_params fed_params[] = {
    /* FRIGATE — small, compact, tight nacelles */
    { 0.55f, 0.08f, 0.12f, 0.4f,
      0.15f, 0.12f, -0.8f, 0.15f,
      0.50f, 0.28f, 0.08f, 0.07f, -0.75f, -0.05f,
      0.25f, 0.10f, 0.08f, false },
    /* DESTROYER — medium, angular */
    { 0.70f, 0.10f, 0.14f, 0.5f,
      0.20f, 0.16f, -1.1f, 0.22f,
      0.68f, 0.34f, 0.10f, 0.08f, -1.0f, -0.02f,
      0.30f, 0.12f, 0.10f, false },
    /* CRUISER — the original model */
    { 0.90f, 0.12f, 0.15f, 0.6f,
      0.25f, 0.20f, -1.4f, 0.30f,
      0.85f, 0.40f, 0.12f, 0.10f, -1.3f, 0.0f,
      0.35f, 0.15f, 0.12f, false },
    /* BATTLECRUISER — massive, extra hull section */
    { 1.15f, 0.14f, 0.18f, 0.7f,
      0.32f, 0.25f, -1.8f, 0.35f,
      1.05f, 0.48f, 0.14f, 0.12f, -1.7f, 0.1f,
      0.42f, 0.18f, 0.14f, true },
};

static void sfa_draw_ship(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                           sfa_ship *s) {
    float h = s->visual_heading;
    int cls = s->ship_class;
    if (cls < 0 || cls >= SHIP_CLASS_COUNT) cls = SHIP_CLASS_CRUISER;
    const fed_ship_params *p = &fed_params[cls];

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
    uint32_t deflector  = 0xFFFFAA33;

    /* ── Saucer section (octagonal prism) ── */
    {
        float sr = p->saucer_r;
        float sh = p->saucer_h;
        float sy = p->saucer_y;
        float sz = p->saucer_z;
        int n = 8;
        float angles[9];
        for (int i = 0; i <= n; i++)
            angles[i] = SFA_TWO_PI * (float)i / (float)n;

        for (int i = 0; i < n; i++) {
            float c0 = cosf(angles[i]),   s0 = sinf(angles[i]);
            float c1 = cosf(angles[i+1]), s1 = sinf(angles[i+1]);
            float x0 = s0 * sr, z0 = c0 * sr + sz;
            float x1 = s1 * sr, z1 = c1 * sr + sz;

            sr_draw_triangle(fb_ptr,
                sr_vert_c(0, sy + sh, sz, 0.5f, 0.5f, hull_top),
                sr_vert_c(x1, sy + sh, z1, 1, 0, hull_top),
                sr_vert_c(x0, sy + sh, z0, 0, 0, hull_top),
                NULL, &mvp);
            sr_draw_triangle(fb_ptr,
                sr_vert_c(0, sy - sh, sz, 0.5f, 0.5f, hull_bot),
                sr_vert_c(x0, sy - sh, z0, 0, 0, hull_bot),
                sr_vert_c(x1, sy - sh, z1, 1, 0, hull_bot),
                NULL, &mvp);
            sr_draw_quad(fb_ptr,
                sr_vert_c(x0, sy - sh, z0, 0, 0, hull_side),
                sr_vert_c(x1, sy - sh, z1, 1, 0, hull_side),
                sr_vert_c(x1, sy + sh, z1, 1, 1, hull_side),
                sr_vert_c(x0, sy + sh, z0, 0, 1, hull_side),
                NULL, &mvp);
        }

        /* Bridge dome */
        sfa_draw_box(fb_ptr, &mvp,
                     -p->bridge_r, sy + sh, sz - p->bridge_r,
                      p->bridge_r, sy + sh + p->bridge_h, sz + p->bridge_r,
                     bridge_col, bridge_col, hull_top);
    }

    /* ── Engineering hull ── */
    {
        float ew = p->eng_hw, eh = p->eng_hh;
        float ey = 0.0f;
        float ez0 = p->eng_z0, ez1 = p->eng_z1;

        sfa_draw_box(fb_ptr, &mvp,
                     -ew, ey - eh, ez0,
                      ew, ey + eh, ez1,
                     hull_top, hull_side, hull_bot);

        /* Deflector dish */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-ew*0.6f, ey - eh*0.6f, ez1 + 0.01f, 0,0, deflector),
            sr_vert_c(-ew*0.6f, ey + eh*0.6f, ez1 + 0.01f, 0,1, deflector),
            sr_vert_c( ew*0.6f, ey + eh*0.6f, ez1 + 0.01f, 1,1, deflector),
            sr_vert_c( ew*0.6f, ey - eh*0.6f, ez1 + 0.01f, 1,0, deflector),
            NULL, &mvp);
    }

    /* ── Battlecruiser secondary hull (below engineering) ── */
    if (p->has_secondary) {
        float sw = p->eng_hw * 0.7f;
        float sh2 = p->eng_hh * 0.5f;
        float sy = -p->eng_hh - sh2;
        sfa_draw_box(fb_ptr, &mvp,
                     -sw, sy - sh2, p->eng_z0 * 0.7f,
                      sw, sy + sh2, p->eng_z1 * 0.5f,
                     hull_side, hull_bot, hull_bot);
    }

    /* ── Nacelle pylons ── */
    {
        float pw = 0.06f;
        float nx = p->nacelle_x;
        float py = p->pylon_top_y;
        float pz0 = (p->nacelle_z0 + p->nacelle_z1) * 0.4f;
        float pz1 = pz0 - 0.3f;

        /* Left pylon */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-p->eng_hw, 0.0f, pz0, 0,0, pylon_col),
            sr_vert_c(-p->eng_hw, 0.0f, pz1, 0,1, pylon_col),
            sr_vert_c(-nx, py, pz1, 1,1, pylon_col),
            sr_vert_c(-nx, py, pz0, 1,0, pylon_col),
            NULL, &mvp);
        sr_draw_quad(fb_ptr,
            sr_vert_c(-p->eng_hw, pw, pz0, 0,0, pylon_col),
            sr_vert_c(-nx, py+pw, pz0, 0,1, pylon_col),
            sr_vert_c(-nx, py+pw, pz1, 1,1, pylon_col),
            sr_vert_c(-p->eng_hw, pw, pz1, 1,0, pylon_col),
            NULL, &mvp);

        /* Right pylon */
        sr_draw_quad(fb_ptr,
            sr_vert_c(p->eng_hw, 0.0f, pz1, 0,0, pylon_col),
            sr_vert_c(p->eng_hw, 0.0f, pz0, 0,1, pylon_col),
            sr_vert_c(nx, py, pz0, 1,1, pylon_col),
            sr_vert_c(nx, py, pz1, 1,0, pylon_col),
            NULL, &mvp);
        sr_draw_quad(fb_ptr,
            sr_vert_c(p->eng_hw, pw, pz1, 0,0, pylon_col),
            sr_vert_c(nx, py+pw, pz1, 0,1, pylon_col),
            sr_vert_c(nx, py+pw, pz0, 1,1, pylon_col),
            sr_vert_c(p->eng_hw, pw, pz0, 1,0, pylon_col),
            NULL, &mvp);
    }

    /* ── Nacelles ── */
    {
        float nw = p->nacelle_hw, nh = p->nacelle_hh;
        float ny = p->nacelle_y;
        float nz0 = p->nacelle_z0, nz1 = p->nacelle_z1;
        float nx = p->nacelle_x;

        sfa_draw_box(fb_ptr, &mvp,
                     -nx - nw, ny - nh, nz0,
                     -nx + nw, ny + nh, nz1,
                     nacelle_t, nacelle_s, nacelle_b);
        sfa_draw_box(fb_ptr, &mvp,
                      nx - nw, ny - nh, nz0,
                      nx + nw, ny + nh, nz1,
                     nacelle_t, nacelle_s, nacelle_b);

        /* Bussard collectors */
        uint32_t bussard = 0xFF2222FF;
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

    /* ── Engine glow ── */
    if (s->current_speed > 0.1f) {
        float speed_frac = s->current_speed / sfa_speed_values[SFA_NUM_SPEEDS - 1];
        float glow_len = 0.4f + 0.9f * speed_frac;
        float pulse = 0.7f + 0.3f * sinf(sfa.time * 12.0f);
        uint8_t gr = (uint8_t)(255.0f * pulse);
        uint8_t gg = (uint8_t)(100.0f * pulse);
        uint32_t glow_col = 0xFF000000 | (uint32_t)(0x22) << 16 | (uint32_t)gg << 8 | gr;

        float nx = p->nacelle_x;
        float ny = p->nacelle_y;
        float nz0 = p->nacelle_z0;
        float gw = p->nacelle_hw * 0.5f;

        /* Left nacelle exhaust */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(-nx, ny + gw, nz0,             0.5f, 0, glow_col),
            sr_vert_c(-nx - gw, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            sr_vert_c(-nx + gw, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            NULL, &mvp);
        sr_draw_triangle(fb_ptr,
            sr_vert_c(-nx, ny - gw, nz0,             0.5f, 0, glow_col),
            sr_vert_c(-nx + gw, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            sr_vert_c(-nx - gw, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            NULL, &mvp);

        /* Right nacelle exhaust */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(nx, ny + gw, nz0,             0.5f, 0, glow_col),
            sr_vert_c(nx + gw, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            sr_vert_c(nx - gw, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            NULL, &mvp);
        sr_draw_triangle(fb_ptr,
            sr_vert_c(nx, ny - gw, nz0,             0.5f, 0, glow_col),
            sr_vert_c(nx - gw, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            sr_vert_c(nx + gw, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            NULL, &mvp);

        /* Main impulse exhaust */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(0, 0.05f, p->eng_z0,                  0.5f, 0, glow_col),
            sr_vert_c(-p->eng_hw*0.5f, 0, p->eng_z0 - glow_len*0.6f, 0, 1, 0xFF000000),
            sr_vert_c( p->eng_hw*0.5f, 0, p->eng_z0 - glow_len*0.6f, 1, 1, 0xFF000000),
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

/* ── Parameterized Klingon ship model ────────────────────────────── */
typedef struct {
    float body_hw;        /* body half-width */
    float body_hh_lo;     /* body Y below center */
    float body_hh_hi;     /* body Y above center */
    float body_z0;        /* body aft Z */
    float body_z1;        /* body fore Z */
    float neck_hw;        /* neck half-width */
    float neck_z1;        /* neck fore Z (connects to head) */
    float head_hw;        /* command pod half-width */
    float head_z0;        /* head aft Z */
    float head_z1;        /* head fore Z */
    float wing_span;      /* wing tip X extent */
    float wing_droop;     /* wing tip Y drop */
    float wing_fwd;       /* wing tip fore Z */
    float wing_aft;       /* wing tip aft Z */
    float gun_size;       /* wingtip gun box half-extent */
    bool  has_torpedo_pod; /* battlecruiser: belly torpedo pod */
} kling_ship_params;

static const kling_ship_params kling_params[] = {
    /* FRIGATE — small raider, tight wings */
    { 0.20f, 0.08f, 0.10f, -0.6f, 0.0f,
      0.06f, 0.45f,
      0.14f, 0.45f, 0.75f,
      0.9f, -0.10f, 0.25f, -0.12f, 0.05f, false },
    /* DESTROYER — the original Bird of Prey */
    { 0.30f, 0.10f, 0.15f, -0.9f, 0.0f,
      0.10f, 0.70f,
      0.20f, 0.70f, 1.10f,
      1.4f, -0.15f, 0.40f, -0.20f, 0.075f, false },
    /* CRUISER — D7-style: wider body, longer neck, big wings */
    { 0.38f, 0.12f, 0.18f, -1.2f, 0.0f,
      0.12f, 0.90f,
      0.25f, 0.90f, 1.35f,
      1.7f, -0.18f, 0.55f, -0.30f, 0.09f, false },
    /* BATTLECRUISER — massive Negh'Var style */
    { 0.45f, 0.15f, 0.22f, -1.6f, 0.0f,
      0.14f, 1.10f,
      0.30f, 1.10f, 1.65f,
      2.0f, -0.22f, 0.70f, -0.40f, 0.10f, true },
};

static void sfa_draw_target_ship(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                   float tx, float tz, float heading, int ship_class) {
    int cls = ship_class;
    if (cls < 0 || cls >= SHIP_CLASS_COUNT) cls = SHIP_CLASS_DESTROYER;
    const kling_ship_params *k = &kling_params[cls];

    uint32_t hull_t = sfa_pal_abgr(30);
    uint32_t hull_s = sfa_pal_abgr(29);
    uint32_t hull_b = sfa_pal_abgr(35);
    uint32_t wing_t = sfa_pal_abgr(36);
    uint32_t wing_s = sfa_pal_abgr(35);
    uint32_t wing_b = sfa_pal_abgr(34);
    uint32_t head_t = sfa_pal_abgr(31);
    uint32_t head_s = sfa_pal_abgr(30);
    uint32_t gun_col = sfa_pal_abgr(15);

    sr_mat4 model = sr_mat4_mul(
        sr_mat4_translate(tx, 0.0f, tz),
        sr_mat4_rotate_y(-heading)
    );
    sr_mat4 mvp = sr_mat4_mul(*vp, model);

    /* Central body */
    sfa_draw_box(fb_ptr, &mvp,
                 -k->body_hw, -k->body_hh_lo, k->body_z0,
                  k->body_hw,  k->body_hh_hi, k->body_z1,
                 hull_t, hull_s, hull_b);

    /* Neck */
    sfa_draw_box(fb_ptr, &mvp,
                 -k->neck_hw, -k->neck_hw*0.5f, k->body_z1,
                  k->neck_hw,  k->neck_hw*0.8f, k->neck_z1,
                 hull_t, hull_s, hull_b);

    /* Command pod */
    sfa_draw_box(fb_ptr, &mvp,
                 -k->head_hw, -k->head_hw*0.4f, k->head_z0,
                  k->head_hw,  k->head_hw*0.6f, k->head_z1,
                 head_t, head_s, hull_b);

    /* Disruptor cannon (front of head) */
    {
        float gw = k->head_hw * 0.4f;
        float gz = k->head_z1 + 0.01f;
        sr_draw_quad(fb_ptr,
            sr_vert_c(-gw, -gw*0.25f, gz, 0,0, gun_col),
            sr_vert_c(-gw,  gw*0.75f, gz, 0,1, gun_col),
            sr_vert_c( gw,  gw*0.75f, gz, 1,1, gun_col),
            sr_vert_c( gw, -gw*0.25f, gz, 1,0, gun_col),
            NULL, &mvp);
    }

    /* Torpedo pod (battlecruiser only) */
    if (k->has_torpedo_pod) {
        float py = -k->body_hh_lo - 0.08f;
        sfa_draw_box(fb_ptr, &mvp,
                     -k->body_hw*0.5f, py - 0.10f, k->body_z0*0.4f,
                      k->body_hw*0.5f, py,         k->body_z1 + 0.3f,
                     hull_s, hull_b, hull_b);
    }

    /* Left swept wing */
    {
        float bx = k->body_hw, bz_aft = k->body_z0*0.67f, bz_fwd = k->body_z1*0.1f;
        float wx = k->wing_span, wd = k->wing_droop;
        float wz_fwd = k->wing_fwd, wz_aft = k->wing_aft;
        float yt = 0.05f, yb = -0.02f;

        /* Top face */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-bx, yt, bz_aft, 0,0, wing_t),
            sr_vert_c(-bx, yt, -bz_fwd, 0,1, wing_t),
            sr_vert_c(-wx, wd, wz_fwd, 1,1, wing_t),
            sr_vert_c(-wx, wd, wz_aft, 1,0, wing_t),
            NULL, &mvp);
        /* Bottom face */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-bx, yb, bz_aft, 0,0, wing_b),
            sr_vert_c(-wx, wd-0.03f, wz_aft, 1,0, wing_b),
            sr_vert_c(-wx, wd-0.03f, wz_fwd, 1,1, wing_b),
            sr_vert_c(-bx, yb, -bz_fwd, 0,1, wing_b),
            NULL, &mvp);
        /* Leading edge */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-bx, yb, -bz_fwd, 0,0, wing_s),
            sr_vert_c(-wx, wd-0.03f, wz_fwd, 1,0, wing_s),
            sr_vert_c(-wx, wd, wz_fwd, 1,1, wing_s),
            sr_vert_c(-bx, yt, -bz_fwd, 0,1, wing_s),
            NULL, &mvp);
        /* Wingtip gun */
        sfa_draw_box(fb_ptr, &mvp,
                     -wx - k->gun_size, wd - k->gun_size, wz_fwd - k->gun_size*2,
                     -wx + k->gun_size, wd + k->gun_size, wz_fwd + k->gun_size*4,
                     gun_col, gun_col, gun_col);
    }

    /* Right swept wing (mirror) */
    {
        float bx = k->body_hw, bz_aft = k->body_z0*0.67f, bz_fwd = k->body_z1*0.1f;
        float wx = k->wing_span, wd = k->wing_droop;
        float wz_fwd = k->wing_fwd, wz_aft = k->wing_aft;
        float yt = 0.05f, yb = -0.02f;

        sr_draw_quad(fb_ptr,
            sr_vert_c(bx, yt, -bz_fwd, 0,0, wing_t),
            sr_vert_c(bx, yt, bz_aft, 0,1, wing_t),
            sr_vert_c(wx, wd, wz_aft, 1,1, wing_t),
            sr_vert_c(wx, wd, wz_fwd, 1,0, wing_t),
            NULL, &mvp);
        sr_draw_quad(fb_ptr,
            sr_vert_c(bx, yb, -bz_fwd, 0,0, wing_b),
            sr_vert_c(bx, yb, bz_aft, 0,1, wing_b),
            sr_vert_c(wx, wd-0.03f, wz_aft, 1,1, wing_b),
            sr_vert_c(wx, wd-0.03f, wz_fwd, 1,0, wing_b),
            NULL, &mvp);
        sr_draw_quad(fb_ptr,
            sr_vert_c(bx, yt, -bz_fwd, 0,0, wing_s),
            sr_vert_c(bx, yb, -bz_fwd, 0,1, wing_s),
            sr_vert_c(wx, wd-0.03f, wz_fwd, 1,1, wing_s),
            sr_vert_c(wx, wd, wz_fwd, 1,0, wing_s),
            NULL, &mvp);
        sfa_draw_box(fb_ptr, &mvp,
                     wx - k->gun_size, wd - k->gun_size, wz_fwd - k->gun_size*2,
                     wx + k->gun_size, wd + k->gun_size, wz_fwd + k->gun_size*4,
                     gun_col, gun_col, gun_col);
    }
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
                                    float heading, int W, int H, int ship_class) {
    /* Build bounding box from Klingon ship params */
    int cls = ship_class;
    if (cls < 0 || cls >= SHIP_CLASS_COUNT) cls = SHIP_CLASS_DESTROYER;
    const kling_ship_params *k = &kling_params[cls];
    float bx = k->wing_span + k->gun_size;
    float by_lo = k->wing_droop - 0.03f;
    float by_hi = k->body_hh_hi;
    float bz_lo = k->body_z0;
    float bz_hi = k->head_z1;
    float bbox_pts[8][3] = {
        {-bx, by_lo, bz_lo}, { bx, by_lo, bz_lo},
        {-bx, by_hi, bz_lo}, { bx, by_hi, bz_lo},
        {-bx, by_lo, bz_hi}, { bx, by_lo, bz_hi},
        {-bx, by_hi, bz_hi}, { bx, by_hi, bz_hi},
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
