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

/* ── Helper: draw octagonal saucer disc ─────────────────────────── */
/* Draws a flat octagonal disc at given center and radius.           */
/* Winding: top face normal points +Y, bottom face normal points -Y */
static void sfa_draw_saucer(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                              float cx, float cy, float cz, float radius, float half_h,
                              uint32_t top_col, uint32_t side_col, uint32_t bot_col) {
    int n = 8;
    for (int i = 0; i < n; i++) {
        float a0 = SFA_TWO_PI * (float)i / (float)n;
        float a1 = SFA_TWO_PI * (float)(i + 1) / (float)n;
        float x0 = sinf(a0) * radius + cx, z0 = cosf(a0) * radius + cz;
        float x1 = sinf(a1) * radius + cx, z1 = cosf(a1) * radius + cz;

        /* Top face — CCW from above (center→v1→v0, angles go CW from above) */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(cx, cy + half_h, cz, 0.5f, 0.5f, top_col),
            sr_vert_c(x1, cy + half_h, z1, 1, 0, top_col),
            sr_vert_c(x0, cy + half_h, z0, 0, 0, top_col),
            NULL, mvp);
        /* Bottom face — CCW from below (center→v0→v1) */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(cx, cy - half_h, cz, 0.5f, 0.5f, bot_col),
            sr_vert_c(x0, cy - half_h, z0, 0, 0, bot_col),
            sr_vert_c(x1, cy - half_h, z1, 1, 0, bot_col),
            NULL, mvp);
        /* Side rim — CCW from outside (bot0→top0→top1→bot1) */
        sr_draw_quad(fb_ptr,
            sr_vert_c(x0, cy - half_h, z0, 0, 0, side_col),
            sr_vert_c(x0, cy + half_h, z0, 0, 1, side_col),
            sr_vert_c(x1, cy + half_h, z1, 1, 1, side_col),
            sr_vert_c(x1, cy - half_h, z1, 1, 0, side_col),
            NULL, mvp);
    }
}

/* ── Helper: draw engine exhaust triangles ──────────────────────── */
static void sfa_draw_exhaust(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                               float ex, float ey, float ez, float hw, float len,
                               uint32_t glow_col) {
    sr_draw_triangle(fb_ptr,
        sr_vert_c(ex, ey + hw, ez,         0.5f, 0, glow_col),
        sr_vert_c(ex - hw, ey, ez - len,   0, 1, 0xFF000000),
        sr_vert_c(ex + hw, ey, ez - len,   1, 1, 0xFF000000),
        NULL, mvp);
    sr_draw_triangle(fb_ptr,
        sr_vert_c(ex, ey - hw, ez,         0.5f, 0, glow_col),
        sr_vert_c(ex + hw, ey, ez - len,   1, 1, 0xFF000000),
        sr_vert_c(ex - hw, ey, ez - len,   0, 1, 0xFF000000),
        NULL, mvp);
}

/* ── Helper: compute engine glow color from speed ───────────────── */
static uint32_t sfa_glow_color(float speed) {
    float frac = speed / sfa_speed_values[SFA_NUM_SPEEDS - 1];
    float pulse = 0.7f + 0.3f * sinf(sfa.time * 12.0f);
    uint8_t r = (uint8_t)(255.0f * pulse);
    uint8_t g = (uint8_t)(100.0f * pulse);
    return 0xFF000000 | (uint32_t)(0x22) << 16 | (uint32_t)g << 8 | r;
}

/* ════════════════════════════════════════════════════════════════════
 *  FRIGATE — Saladin class
 *  Small saucer, integrated compact hull beneath, single dorsal nacelle.
 *  Fast and agile, minimal profile.
 *
 *  Side view:         ___nacelle___
 *                     |  pylon  |
 *           ====saucer====
 *              [hull]
 *
 *  Top view:    ====O====      (O = bridge)
 *               [  hull ]
 *                  |n|         (single nacelle centered)
 * ════════════════════════════════════════════════════════════════════ */
static void sfa_draw_fed_frigate(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                   uint32_t hull_t, uint32_t hull_s, uint32_t hull_b,
                                   uint32_t nac_t, uint32_t nac_s, uint32_t nac_b,
                                   uint32_t pylon_col, uint32_t bridge_col, uint32_t deflector,
                                   float speed) {
    /* Saucer — small hex-ish disc, 6 sides */
    sfa_draw_saucer(fb_ptr, mvp, 0, 0.10f, 0.30f, 0.50f, 0.06f,
                    hull_t, hull_s, hull_b);

    /* Bridge */
    sfa_draw_box(fb_ptr, mvp, -0.08f, 0.16f, 0.24f, 0.08f, 0.22f, 0.36f,
                 bridge_col, bridge_col, hull_t);

    /* Integrated hull — short, directly under saucer */
    sfa_draw_box(fb_ptr, mvp, -0.14f, -0.10f, -0.55f, 0.14f, 0.06f, 0.15f,
                 hull_t, hull_s, hull_b);

    /* Deflector */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.09f, -0.06f, 0.16f, 0,0, deflector),
        sr_vert_c(-0.09f,  0.02f, 0.16f, 0,1, deflector),
        sr_vert_c( 0.09f,  0.02f, 0.16f, 1,1, deflector),
        sr_vert_c( 0.09f, -0.06f, 0.16f, 1,0, deflector),
        NULL, mvp);

    /* Single dorsal pylon */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.04f, 0.06f, -0.30f, 0,0, pylon_col),
        sr_vert_c(-0.04f, 0.35f, -0.30f, 0,1, pylon_col),
        sr_vert_c(-0.04f, 0.35f, -0.10f, 1,1, pylon_col),
        sr_vert_c(-0.04f, 0.06f, -0.10f, 1,0, pylon_col),
        NULL, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c( 0.04f, 0.06f, -0.10f, 0,0, pylon_col),
        sr_vert_c( 0.04f, 0.35f, -0.10f, 0,1, pylon_col),
        sr_vert_c( 0.04f, 0.35f, -0.30f, 1,1, pylon_col),
        sr_vert_c( 0.04f, 0.06f, -0.30f, 1,0, pylon_col),
        NULL, mvp);

    /* Single nacelle — centered on dorsal pylon */
    sfa_draw_box(fb_ptr, mvp, -0.08f, 0.30f, -0.60f, 0.08f, 0.42f, 0.05f,
                 nac_t, nac_s, nac_b);

    /* Bussard collector */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.08f, 0.30f, 0.06f, 0,0, 0xFF2222FF),
        sr_vert_c(-0.08f, 0.42f, 0.06f, 0,1, 0xFF2222FF),
        sr_vert_c( 0.08f, 0.42f, 0.06f, 1,1, 0xFF2222FF),
        sr_vert_c( 0.08f, 0.30f, 0.06f, 1,0, 0xFF2222FF),
        NULL, mvp);

    /* Engine glow */
    if (speed > 0.1f) {
        uint32_t gc = sfa_glow_color(speed);
        float gl = 0.4f + 0.5f * (speed / sfa_speed_values[SFA_NUM_SPEEDS - 1]);
        sfa_draw_exhaust(fb_ptr, mvp, 0, 0.36f, -0.60f, 0.04f, gl, gc);
        sfa_draw_exhaust(fb_ptr, mvp, 0, -0.02f, -0.55f, 0.06f, gl * 0.5f, gc);
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  DESTROYER — Miranda class
 *  Medium saucer with engineering hull directly attached (no neck).
 *  Two nacelles at hull level on short stub pylons.
 *  Distinctive weapons rollbar arch over the top.
 *
 *  Side view:    [rollbar pod]
 *                 |        |
 *           ====saucer+hull====
 *           nac──┘        └──nac
 *
 *  Top view:       ====O====
 *            nac──[  hull   ]──nac
 *                  [rollbar]
 * ════════════════════════════════════════════════════════════════════ */
static void sfa_draw_fed_destroyer(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                     uint32_t hull_t, uint32_t hull_s, uint32_t hull_b,
                                     uint32_t nac_t, uint32_t nac_s, uint32_t nac_b,
                                     uint32_t pylon_col, uint32_t bridge_col, uint32_t deflector,
                                     float speed) {
    /* Saucer */
    sfa_draw_saucer(fb_ptr, mvp, 0, 0.12f, 0.35f, 0.65f, 0.08f,
                    hull_t, hull_s, hull_b);

    /* Bridge */
    sfa_draw_box(fb_ptr, mvp, -0.10f, 0.20f, 0.28f, 0.10f, 0.28f, 0.42f,
                 bridge_col, bridge_col, hull_t);

    /* Engineering hull — directly behind saucer, no neck */
    sfa_draw_box(fb_ptr, mvp, -0.22f, -0.12f, -0.70f, 0.22f, 0.10f, 0.10f,
                 hull_t, hull_s, hull_b);

    /* Deflector */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.12f, -0.08f, 0.11f, 0,0, deflector),
        sr_vert_c(-0.12f,  0.04f, 0.11f, 0,1, deflector),
        sr_vert_c( 0.12f,  0.04f, 0.11f, 1,1, deflector),
        sr_vert_c( 0.12f, -0.08f, 0.11f, 1,0, deflector),
        NULL, mvp);

    /* Nacelles — at hull level on short stub pylons */
    float nx = 0.55f, ny = 0.0f;

    /* Left pylon (short horizontal strut) */
    sfa_draw_box(fb_ptr, mvp,
                 -0.22f, -0.04f, -0.50f,
                 -nx + 0.08f, 0.04f, -0.30f,
                 pylon_col, pylon_col, pylon_col);
    /* Right pylon */
    sfa_draw_box(fb_ptr, mvp,
                 nx - 0.08f, -0.04f, -0.50f,
                 0.22f, 0.04f, -0.30f,
                 pylon_col, pylon_col, pylon_col);

    /* Left nacelle */
    sfa_draw_box(fb_ptr, mvp,
                 -nx - 0.08f, ny - 0.07f, -0.70f,
                 -nx + 0.08f, ny + 0.07f, -0.10f,
                 nac_t, nac_s, nac_b);
    /* Right nacelle */
    sfa_draw_box(fb_ptr, mvp,
                 nx - 0.08f, ny - 0.07f, -0.70f,
                 nx + 0.08f, ny + 0.07f, -0.10f,
                 nac_t, nac_s, nac_b);

    /* Bussard collectors */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-nx-0.08f, ny-0.07f, -0.09f, 0,0, 0xFF2222FF),
        sr_vert_c(-nx-0.08f, ny+0.07f, -0.09f, 0,1, 0xFF2222FF),
        sr_vert_c(-nx+0.08f, ny+0.07f, -0.09f, 1,1, 0xFF2222FF),
        sr_vert_c(-nx+0.08f, ny-0.07f, -0.09f, 1,0, 0xFF2222FF),
        NULL, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(nx-0.08f, ny-0.07f, -0.09f, 0,0, 0xFF2222FF),
        sr_vert_c(nx-0.08f, ny+0.07f, -0.09f, 0,1, 0xFF2222FF),
        sr_vert_c(nx+0.08f, ny+0.07f, -0.09f, 1,1, 0xFF2222FF),
        sr_vert_c(nx+0.08f, ny-0.07f, -0.09f, 1,0, 0xFF2222FF),
        NULL, mvp);

    /* Weapons rollbar — arch over the saucer */
    {
        float ry = 0.40f;  /* rollbar height */
        /* Left vertical strut */
        sfa_draw_box(fb_ptr, mvp,
                     -0.38f, 0.15f, -0.30f,
                     -0.34f, ry, -0.25f,
                     pylon_col, pylon_col, pylon_col);
        /* Right vertical strut */
        sfa_draw_box(fb_ptr, mvp,
                     0.34f, 0.15f, -0.30f,
                     0.38f, ry, -0.25f,
                     pylon_col, pylon_col, pylon_col);
        /* Horizontal bar */
        sfa_draw_box(fb_ptr, mvp,
                     -0.38f, ry - 0.04f, -0.32f,
                     0.38f, ry + 0.04f, -0.22f,
                     hull_s, pylon_col, pylon_col);
        /* Torpedo pod (center of rollbar) */
        sfa_draw_box(fb_ptr, mvp,
                     -0.10f, ry - 0.06f, -0.36f,
                     0.10f, ry + 0.06f, -0.18f,
                     0xFF4444CC, 0xFF3333AA, 0xFF222288);
    }

    /* Engine glow */
    if (speed > 0.1f) {
        uint32_t gc = sfa_glow_color(speed);
        float gl = 0.4f + 0.5f * (speed / sfa_speed_values[SFA_NUM_SPEEDS - 1]);
        sfa_draw_exhaust(fb_ptr, mvp, -nx, ny, -0.70f, 0.04f, gl, gc);
        sfa_draw_exhaust(fb_ptr, mvp,  nx, ny, -0.70f, 0.04f, gl, gc);
        sfa_draw_exhaust(fb_ptr, mvp, 0, -0.02f, -0.70f, 0.08f, gl * 0.5f, gc);
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  CRUISER — Constitution class (the classic)
 *  Large saucer connected by neck to cylindrical engineering hull.
 *  Two nacelles elevated on swept-back diagonal pylons.
 *
 *  Side view:                nac
 *                           / |
 *           ====saucer====  pylon
 *                |neck|    / |
 *                [eng hull]  nac
 *
 *  Top view:     nac─────\
 *                ====O====─[eng]
 *                nac─────/
 * ════════════════════════════════════════════════════════════════════ */
static void sfa_draw_fed_cruiser(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                   uint32_t hull_t, uint32_t hull_s, uint32_t hull_b,
                                   uint32_t nac_t, uint32_t nac_s, uint32_t nac_b,
                                   uint32_t pylon_col, uint32_t bridge_col, uint32_t deflector,
                                   float speed) {
    /* Saucer — large disc */
    sfa_draw_saucer(fb_ptr, mvp, 0, 0.15f, 0.60f, 0.85f, 0.10f,
                    hull_t, hull_s, hull_b);

    /* Bridge */
    sfa_draw_box(fb_ptr, mvp, -0.14f, 0.25f, 0.52f, 0.14f, 0.34f, 0.68f,
                 bridge_col, bridge_col, hull_t);

    /* Neck — connecting saucer to engineering hull */
    sfa_draw_box(fb_ptr, mvp, -0.12f, -0.05f, -0.10f, 0.12f, 0.10f, 0.15f,
                 hull_t, hull_s, hull_b);

    /* Engineering hull */
    sfa_draw_box(fb_ptr, mvp, -0.22f, -0.18f, -1.30f, 0.22f, 0.08f, -0.10f,
                 hull_t, hull_s, hull_b);

    /* Deflector dish */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.14f, -0.14f, -0.09f, 0,0, deflector),
        sr_vert_c(-0.14f,  0.04f, -0.09f, 0,1, deflector),
        sr_vert_c( 0.14f,  0.04f, -0.09f, 1,1, deflector),
        sr_vert_c( 0.14f, -0.14f, -0.09f, 1,0, deflector),
        NULL, mvp);

    /* Nacelle pylons — diagonal struts from engineering hull up to nacelles */
    float nx = 0.80f, npy = 0.38f;

    /* Left pylon (front face) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.22f, 0.0f, -0.55f, 0,0, pylon_col),
        sr_vert_c(-0.22f, 0.0f, -0.85f, 0,1, pylon_col),
        sr_vert_c(-nx, npy, -0.85f, 1,1, pylon_col),
        sr_vert_c(-nx, npy, -0.55f, 1,0, pylon_col),
        NULL, mvp);
    /* Left pylon top face */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.22f, 0.04f, -0.55f, 0,0, pylon_col),
        sr_vert_c(-nx, npy+0.04f, -0.55f, 0,1, pylon_col),
        sr_vert_c(-nx, npy+0.04f, -0.85f, 1,1, pylon_col),
        sr_vert_c(-0.22f, 0.04f, -0.85f, 1,0, pylon_col),
        NULL, mvp);

    /* Right pylon */
    sr_draw_quad(fb_ptr,
        sr_vert_c(0.22f, 0.0f, -0.85f, 0,0, pylon_col),
        sr_vert_c(0.22f, 0.0f, -0.55f, 0,1, pylon_col),
        sr_vert_c(nx, npy, -0.55f, 1,1, pylon_col),
        sr_vert_c(nx, npy, -0.85f, 1,0, pylon_col),
        NULL, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(0.22f, 0.04f, -0.85f, 0,0, pylon_col),
        sr_vert_c(nx, npy+0.04f, -0.85f, 0,1, pylon_col),
        sr_vert_c(nx, npy+0.04f, -0.55f, 1,1, pylon_col),
        sr_vert_c(0.22f, 0.04f, -0.55f, 1,0, pylon_col),
        NULL, mvp);

    /* Nacelles */
    sfa_draw_box(fb_ptr, mvp,
                 -nx-0.10f, npy-0.08f, -1.20f,
                 -nx+0.10f, npy+0.08f, -0.05f,
                 nac_t, nac_s, nac_b);
    sfa_draw_box(fb_ptr, mvp,
                 nx-0.10f, npy-0.08f, -1.20f,
                 nx+0.10f, npy+0.08f, -0.05f,
                 nac_t, nac_s, nac_b);

    /* Bussard collectors */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-nx-0.10f, npy-0.08f, -0.04f, 0,0, 0xFF2222FF),
        sr_vert_c(-nx-0.10f, npy+0.08f, -0.04f, 0,1, 0xFF2222FF),
        sr_vert_c(-nx+0.10f, npy+0.08f, -0.04f, 1,1, 0xFF2222FF),
        sr_vert_c(-nx+0.10f, npy-0.08f, -0.04f, 1,0, 0xFF2222FF),
        NULL, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(nx-0.10f, npy-0.08f, -0.04f, 0,0, 0xFF2222FF),
        sr_vert_c(nx-0.10f, npy+0.08f, -0.04f, 0,1, 0xFF2222FF),
        sr_vert_c(nx+0.10f, npy+0.08f, -0.04f, 1,1, 0xFF2222FF),
        sr_vert_c(nx+0.10f, npy-0.08f, -0.04f, 1,0, 0xFF2222FF),
        NULL, mvp);

    /* Engine glow */
    if (speed > 0.1f) {
        uint32_t gc = sfa_glow_color(speed);
        float gl = 0.4f + 0.7f * (speed / sfa_speed_values[SFA_NUM_SPEEDS - 1]);
        sfa_draw_exhaust(fb_ptr, mvp, -nx, npy, -1.20f, 0.05f, gl, gc);
        sfa_draw_exhaust(fb_ptr, mvp,  nx, npy, -1.20f, 0.05f, gl, gc);
        sfa_draw_exhaust(fb_ptr, mvp, 0, -0.05f, -1.30f, 0.10f, gl * 0.5f, gc);
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  BATTLECRUISER — Excelsior class
 *  Large elongated saucer that tapers into engineering hull (no neck).
 *  Two massive nacelles level with secondary hull on horizontal pylons.
 *  Secondary torpedo pod. Heavier, more aggressive Federation design.
 *
 *  Side view:
 *           =====saucer=======eng= nac
 *              [torp pod]          nac
 *
 *  Top view:     nac──────\
 *         ======O===========[eng]
 *                nac──────/
 * ════════════════════════════════════════════════════════════════════ */
static void sfa_draw_fed_battlecruiser(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                         uint32_t hull_t, uint32_t hull_s, uint32_t hull_b,
                                         uint32_t nac_t, uint32_t nac_s, uint32_t nac_b,
                                         uint32_t pylon_col, uint32_t bridge_col, uint32_t deflector,
                                         float speed) {
    /* Saucer — large, elongated (wider front, tapers to rear) */
    sfa_draw_saucer(fb_ptr, mvp, 0, 0.16f, 0.55f, 0.90f, 0.10f,
                    hull_t, hull_s, hull_b);

    /* Bridge — wider command module */
    sfa_draw_box(fb_ptr, mvp, -0.16f, 0.26f, 0.46f, 0.16f, 0.36f, 0.64f,
                 bridge_col, bridge_col, hull_t);

    /* Engineering hull — long, integrated behind saucer (no neck gap) */
    sfa_draw_box(fb_ptr, mvp, -0.28f, -0.20f, -1.70f, 0.28f, 0.10f, -0.10f,
                 hull_t, hull_s, hull_b);

    /* Saucer-hull transition wedge — tapered connection */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.28f, 0.06f, -0.10f, 0,0, hull_s),
        sr_vert_c(-0.50f, 0.06f, 0.10f, 0,1, hull_s),
        sr_vert_c(-0.50f, 0.16f, 0.10f, 1,1, hull_s),
        sr_vert_c(-0.28f, 0.16f, -0.10f, 1,0, hull_s),
        NULL, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(0.28f, 0.06f, -0.10f, 0,0, hull_s),
        sr_vert_c(0.28f, 0.16f, -0.10f, 0,1, hull_s),
        sr_vert_c(0.50f, 0.16f, 0.10f, 1,1, hull_s),
        sr_vert_c(0.50f, 0.06f, 0.10f, 1,0, hull_s),
        NULL, mvp);

    /* Deflector dish — larger */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.18f, -0.16f, -0.09f, 0,0, deflector),
        sr_vert_c(-0.18f,  0.06f, -0.09f, 0,1, deflector),
        sr_vert_c( 0.18f,  0.06f, -0.09f, 1,1, deflector),
        sr_vert_c( 0.18f, -0.16f, -0.09f, 1,0, deflector),
        NULL, mvp);

    /* Torpedo pod — underslung below saucer */
    sfa_draw_box(fb_ptr, mvp,
                 -0.12f, -0.12f, 0.25f,
                 0.12f, 0.02f, 0.60f,
                 hull_s, hull_b, hull_b);
    /* Torpedo tube faces */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.06f, -0.08f, 0.61f, 0,0, 0xFF4444CC),
        sr_vert_c(-0.06f, -0.02f, 0.61f, 0,1, 0xFF4444CC),
        sr_vert_c( 0.06f, -0.02f, 0.61f, 1,1, 0xFF4444CC),
        sr_vert_c( 0.06f, -0.08f, 0.61f, 1,0, 0xFF4444CC),
        NULL, mvp);

    /* Nacelle pylons — horizontal, nacelles level with secondary hull */
    float nx = 0.95f, npy = -0.05f;

    /* Left pylon (front face — horizontal) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.28f, npy, -0.80f, 0,0, pylon_col),
        sr_vert_c(-0.28f, npy, -1.20f, 0,1, pylon_col),
        sr_vert_c(-nx, npy, -1.20f, 1,1, pylon_col),
        sr_vert_c(-nx, npy, -0.80f, 1,0, pylon_col),
        NULL, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.28f, npy+0.05f, -0.80f, 0,0, pylon_col),
        sr_vert_c(-nx, npy+0.05f, -0.80f, 0,1, pylon_col),
        sr_vert_c(-nx, npy+0.05f, -1.20f, 1,1, pylon_col),
        sr_vert_c(-0.28f, npy+0.05f, -1.20f, 1,0, pylon_col),
        NULL, mvp);

    /* Right pylon (front face — horizontal) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(0.28f, npy, -1.20f, 0,0, pylon_col),
        sr_vert_c(0.28f, npy, -0.80f, 0,1, pylon_col),
        sr_vert_c(nx, npy, -0.80f, 1,1, pylon_col),
        sr_vert_c(nx, npy, -1.20f, 1,0, pylon_col),
        NULL, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(0.28f, npy+0.05f, -1.20f, 0,0, pylon_col),
        sr_vert_c(nx, npy+0.05f, -1.20f, 0,1, pylon_col),
        sr_vert_c(nx, npy+0.05f, -0.80f, 1,1, pylon_col),
        sr_vert_c(0.28f, npy+0.05f, -0.80f, 1,0, pylon_col),
        NULL, mvp);

    /* Nacelles — large */
    sfa_draw_box(fb_ptr, mvp,
                 -nx-0.12f, npy-0.10f, -1.60f,
                 -nx+0.12f, npy+0.10f, -0.15f,
                 nac_t, nac_s, nac_b);
    sfa_draw_box(fb_ptr, mvp,
                 nx-0.12f, npy-0.10f, -1.60f,
                 nx+0.12f, npy+0.10f, -0.15f,
                 nac_t, nac_s, nac_b);

    /* Bussard collectors */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-nx-0.12f, npy-0.10f, -0.14f, 0,0, 0xFF2222FF),
        sr_vert_c(-nx-0.12f, npy+0.10f, -0.14f, 0,1, 0xFF2222FF),
        sr_vert_c(-nx+0.12f, npy+0.10f, -0.14f, 1,1, 0xFF2222FF),
        sr_vert_c(-nx+0.12f, npy-0.10f, -0.14f, 1,0, 0xFF2222FF),
        NULL, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(nx-0.12f, npy-0.10f, -0.14f, 0,0, 0xFF2222FF),
        sr_vert_c(nx-0.12f, npy+0.10f, -0.14f, 0,1, 0xFF2222FF),
        sr_vert_c(nx+0.12f, npy+0.10f, -0.14f, 1,1, 0xFF2222FF),
        sr_vert_c(nx+0.12f, npy-0.10f, -0.14f, 1,0, 0xFF2222FF),
        NULL, mvp);

    /* Engine glow */
    if (speed > 0.1f) {
        uint32_t gc = sfa_glow_color(speed);
        float gl = 0.4f + 0.8f * (speed / sfa_speed_values[SFA_NUM_SPEEDS - 1]);
        sfa_draw_exhaust(fb_ptr, mvp, -nx, npy, -1.60f, 0.06f, gl, gc);
        sfa_draw_exhaust(fb_ptr, mvp,  nx, npy, -1.60f, 0.06f, gl, gc);
        sfa_draw_exhaust(fb_ptr, mvp, 0, -0.08f, -1.70f, 0.12f, gl * 0.5f, gc);
    }
}

/* ── Federation ship dispatcher ─────────────────────────────────── */

static void sfa_draw_ship(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                           sfa_ship *s) {
    float h = s->visual_heading;
    int cls = s->ship_class;
    if (cls < 0 || cls >= SHIP_CLASS_COUNT) cls = SHIP_CLASS_CRUISER;

    sr_mat4 model = sr_mat4_mul(
        sr_mat4_translate(s->x, 0.0f, s->z),
        sr_mat4_rotate_y(-h)
    );
    sr_mat4 mvp = sr_mat4_mul(*vp, model);

    /* Colors */
    uint32_t hull_top   = s->color_hull   ? s->color_hull   : SFA_SHIP_COLOR;
    uint32_t nacelle_t  = s->color_accent ? s->color_accent : SFA_SHIP_ACCENT;
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

    switch (cls) {
        case SHIP_CLASS_FRIGATE:
            sfa_draw_fed_frigate(fb_ptr, &mvp, hull_top, hull_side, hull_bot,
                                 nacelle_t, nacelle_s, nacelle_b,
                                 pylon_col, bridge_col, deflector, s->current_speed);
            break;
        case SHIP_CLASS_DESTROYER:
            sfa_draw_fed_destroyer(fb_ptr, &mvp, hull_top, hull_side, hull_bot,
                                    nacelle_t, nacelle_s, nacelle_b,
                                    pylon_col, bridge_col, deflector, s->current_speed);
            break;
        case SHIP_CLASS_CRUISER:
            sfa_draw_fed_cruiser(fb_ptr, &mvp, hull_top, hull_side, hull_bot,
                                  nacelle_t, nacelle_s, nacelle_b,
                                  pylon_col, bridge_col, deflector, s->current_speed);
            break;
        case SHIP_CLASS_BATTLECRUISER:
            sfa_draw_fed_battlecruiser(fb_ptr, &mvp, hull_top, hull_side, hull_bot,
                                        nacelle_t, nacelle_s, nacelle_b,
                                        pylon_col, bridge_col, deflector, s->current_speed);
            break;
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

/* ── Helper: draw Klingon swept wing pair ───────────────────────── */
static void sfa_draw_klingon_wings(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                     float body_hw, float body_z_aft, float body_z_fwd,
                                     float wing_x, float wing_droop,
                                     float wing_z_fwd, float wing_z_aft,
                                     float gun_size,
                                     uint32_t wing_t, uint32_t wing_s, uint32_t wing_b,
                                     uint32_t gun_col) {
    float bx = body_hw;
    float bz_a = body_z_aft * 0.67f, bz_f = body_z_fwd * 0.1f;
    float yt = 0.05f, yb = -0.02f;
    float wd = wing_droop;

    /* Left wing — top */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-bx, yt, bz_a, 0,0, wing_t),
        sr_vert_c(-bx, yt, -bz_f, 0,1, wing_t),
        sr_vert_c(-wing_x, wd, wing_z_fwd, 1,1, wing_t),
        sr_vert_c(-wing_x, wd, wing_z_aft, 1,0, wing_t),
        NULL, mvp);
    /* Left wing — bottom */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-bx, yb, bz_a, 0,0, wing_b),
        sr_vert_c(-wing_x, wd-0.03f, wing_z_aft, 1,0, wing_b),
        sr_vert_c(-wing_x, wd-0.03f, wing_z_fwd, 1,1, wing_b),
        sr_vert_c(-bx, yb, -bz_f, 0,1, wing_b),
        NULL, mvp);
    /* Left wing — leading edge */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-bx, yb, -bz_f, 0,0, wing_s),
        sr_vert_c(-wing_x, wd-0.03f, wing_z_fwd, 1,0, wing_s),
        sr_vert_c(-wing_x, wd, wing_z_fwd, 1,1, wing_s),
        sr_vert_c(-bx, yt, -bz_f, 0,1, wing_s),
        NULL, mvp);
    /* Left wingtip gun */
    sfa_draw_box(fb_ptr, mvp,
                 -wing_x - gun_size, wd - gun_size, wing_z_fwd - gun_size*2,
                 -wing_x + gun_size, wd + gun_size, wing_z_fwd + gun_size*4,
                 gun_col, gun_col, gun_col);

    /* Right wing — top */
    sr_draw_quad(fb_ptr,
        sr_vert_c(bx, yt, -bz_f, 0,0, wing_t),
        sr_vert_c(bx, yt, bz_a, 0,1, wing_t),
        sr_vert_c(wing_x, wd, wing_z_aft, 1,1, wing_t),
        sr_vert_c(wing_x, wd, wing_z_fwd, 1,0, wing_t),
        NULL, mvp);
    /* Right wing — bottom */
    sr_draw_quad(fb_ptr,
        sr_vert_c(bx, yb, -bz_f, 0,0, wing_b),
        sr_vert_c(bx, yb, bz_a, 0,1, wing_b),
        sr_vert_c(wing_x, wd-0.03f, wing_z_aft, 1,1, wing_b),
        sr_vert_c(wing_x, wd-0.03f, wing_z_fwd, 1,0, wing_b),
        NULL, mvp);
    /* Right wing — leading edge */
    sr_draw_quad(fb_ptr,
        sr_vert_c(bx, yt, -bz_f, 0,0, wing_s),
        sr_vert_c(bx, yb, -bz_f, 0,1, wing_s),
        sr_vert_c(wing_x, wd-0.03f, wing_z_fwd, 1,1, wing_s),
        sr_vert_c(wing_x, wd, wing_z_fwd, 1,0, wing_s),
        NULL, mvp);
    /* Right wingtip gun */
    sfa_draw_box(fb_ptr, mvp,
                 wing_x - gun_size, wd - gun_size, wing_z_fwd - gun_size*2,
                 wing_x + gun_size, wd + gun_size, wing_z_fwd + gun_size*4,
                 gun_col, gun_col, gun_col);
}

/* ════════════════════════════════════════════════════════════════════
 *  KLINGON FRIGATE — B'rel scout
 *  Tiny raider: compact body, narrow swept wings, small head pod.
 *  No neck section — head attaches directly to body.
 * ════════════════════════════════════════════════════════════════════ */
static void sfa_draw_kling_frigate(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                     uint32_t hull_t, uint32_t hull_s, uint32_t hull_b,
                                     uint32_t wing_t, uint32_t wing_s, uint32_t wing_b,
                                     uint32_t head_t, uint32_t head_s, uint32_t gun_col) {
    /* Compact body */
    sfa_draw_box(fb_ptr, mvp, -0.16f, -0.06f, -0.45f, 0.16f, 0.10f, 0.0f,
                 hull_t, hull_s, hull_b);
    /* Head pod — directly on front of body, no neck */
    sfa_draw_box(fb_ptr, mvp, -0.12f, -0.04f, 0.0f, 0.12f, 0.08f, 0.50f,
                 head_t, head_s, hull_b);
    /* Disruptor cannon */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.05f, -0.01f, 0.51f, 0,0, gun_col),
        sr_vert_c(-0.05f,  0.05f, 0.51f, 0,1, gun_col),
        sr_vert_c( 0.05f,  0.05f, 0.51f, 1,1, gun_col),
        sr_vert_c( 0.05f, -0.01f, 0.51f, 1,0, gun_col),
        NULL, mvp);

    /* Narrow swept wings */
    sfa_draw_klingon_wings(fb_ptr, mvp,
        0.16f, -0.45f, 0.0f,   /* body extents */
        0.75f, -0.08f,          /* wing span, droop */
        0.20f, -0.08f,          /* wing Z fwd/aft */
        0.04f,                   /* gun size */
        wing_t, wing_s, wing_b, gun_col);
}

/* ════════════════════════════════════════════════════════════════════
 *  KLINGON DESTROYER — Bird of Prey (classic)
 *  Aft body + neck + command pod. Dramatic swept wings with droop.
 *  The iconic Klingon silhouette.
 * ════════════════════════════════════════════════════════════════════ */
static void sfa_draw_kling_destroyer(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                       uint32_t hull_t, uint32_t hull_s, uint32_t hull_b,
                                       uint32_t wing_t, uint32_t wing_s, uint32_t wing_b,
                                       uint32_t head_t, uint32_t head_s, uint32_t gun_col) {
    /* Central body */
    sfa_draw_box(fb_ptr, mvp, -0.28f, -0.10f, -0.85f, 0.28f, 0.14f, 0.0f,
                 hull_t, hull_s, hull_b);
    /* Neck */
    sfa_draw_box(fb_ptr, mvp, -0.08f, -0.04f, 0.0f, 0.08f, 0.07f, 0.65f,
                 hull_t, hull_s, hull_b);
    /* Command pod */
    sfa_draw_box(fb_ptr, mvp, -0.18f, -0.07f, 0.65f, 0.18f, 0.11f, 1.05f,
                 head_t, head_s, hull_b);
    /* Disruptor */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.07f, -0.02f, 1.06f, 0,0, gun_col),
        sr_vert_c(-0.07f,  0.06f, 1.06f, 0,1, gun_col),
        sr_vert_c( 0.07f,  0.06f, 1.06f, 1,1, gun_col),
        sr_vert_c( 0.07f, -0.02f, 1.06f, 1,0, gun_col),
        NULL, mvp);

    /* Classic swept wings */
    sfa_draw_klingon_wings(fb_ptr, mvp,
        0.28f, -0.85f, 0.0f,
        1.30f, -0.14f,
        0.35f, -0.18f,
        0.07f,
        wing_t, wing_s, wing_b, gun_col);
}

/* ════════════════════════════════════════════════════════════════════
 *  KLINGON CRUISER — D7/K't'inga class
 *  Bulky aft engineering section with engine pods on sides.
 *  Short wide neck leading to large command bulb.
 *  Shorter, wider wings than the BoP — more angular.
 * ════════════════════════════════════════════════════════════════════ */
static void sfa_draw_kling_cruiser(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                     uint32_t hull_t, uint32_t hull_s, uint32_t hull_b,
                                     uint32_t wing_t, uint32_t wing_s, uint32_t wing_b,
                                     uint32_t head_t, uint32_t head_s, uint32_t gun_col) {
    /* Bulky engineering section */
    sfa_draw_box(fb_ptr, mvp, -0.35f, -0.14f, -1.10f, 0.35f, 0.18f, -0.10f,
                 hull_t, hull_s, hull_b);

    /* Engine nacelle pods — flanking the body */
    sfa_draw_box(fb_ptr, mvp, -0.50f, -0.08f, -1.00f, -0.35f, 0.12f, -0.40f,
                 hull_s, hull_b, hull_b);
    sfa_draw_box(fb_ptr, mvp,  0.35f, -0.08f, -1.00f,  0.50f, 0.12f, -0.40f,
                 hull_s, hull_b, hull_b);
    /* Engine glows */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.50f, -0.04f, -1.01f, 0,0, 0xFF2244AA),
        sr_vert_c(-0.50f,  0.08f, -1.01f, 0,1, 0xFF2244AA),
        sr_vert_c(-0.35f,  0.08f, -1.01f, 1,1, 0xFF2244AA),
        sr_vert_c(-0.35f, -0.04f, -1.01f, 1,0, 0xFF2244AA),
        NULL, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c( 0.35f, -0.04f, -1.01f, 0,0, 0xFF2244AA),
        sr_vert_c( 0.35f,  0.08f, -1.01f, 0,1, 0xFF2244AA),
        sr_vert_c( 0.50f,  0.08f, -1.01f, 1,1, 0xFF2244AA),
        sr_vert_c( 0.50f, -0.04f, -1.01f, 1,0, 0xFF2244AA),
        NULL, mvp);

    /* Wider neck */
    sfa_draw_box(fb_ptr, mvp, -0.12f, -0.06f, -0.10f, 0.12f, 0.08f, 0.75f,
                 hull_t, hull_s, hull_b);

    /* Large command bulb — wider, more angular */
    sfa_draw_box(fb_ptr, mvp, -0.24f, -0.10f, 0.75f, 0.24f, 0.14f, 1.30f,
                 head_t, head_s, hull_b);
    /* Torpedo launcher (underside of command bulb) */
    sfa_draw_box(fb_ptr, mvp, -0.08f, -0.16f, 0.90f, 0.08f, -0.10f, 1.20f,
                 hull_s, hull_b, hull_b);
    /* Disruptor array */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.10f, -0.02f, 1.31f, 0,0, gun_col),
        sr_vert_c(-0.10f,  0.08f, 1.31f, 0,1, gun_col),
        sr_vert_c( 0.10f,  0.08f, 1.31f, 1,1, gun_col),
        sr_vert_c( 0.10f, -0.02f, 1.31f, 1,0, gun_col),
        NULL, mvp);

    /* D7-style wings — straight out (no sweep), pitched ~10 degrees down */
    sfa_draw_klingon_wings(fb_ptr, mvp,
        0.35f, -1.10f, -0.10f,
        1.50f, -0.20f,
        -0.10f, -0.70f,
        0.08f,
        wing_t, wing_s, wing_b, gun_col);
}

/* ════════════════════════════════════════════════════════════════════
 *  KLINGON BATTLECRUISER — Vor'cha / Negh'Var class
 *  Massive engineering hull with armored upper deck.
 *  Elongated forward section (no separate head — integrated).
 *  Very wide wings with heavy weapons. Belly torpedo pod.
 * ════════════════════════════════════════════════════════════════════ */
static void sfa_draw_kling_battlecruiser(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                           uint32_t hull_t, uint32_t hull_s, uint32_t hull_b,
                                           uint32_t wing_t, uint32_t wing_s, uint32_t wing_b,
                                           uint32_t head_t, uint32_t head_s, uint32_t gun_col) {
    /* Massive aft engineering hull */
    sfa_draw_box(fb_ptr, mvp, -0.42f, -0.18f, -1.50f, 0.42f, 0.22f, -0.20f,
                 hull_t, hull_s, hull_b);

    /* Armored upper deck */
    sfa_draw_box(fb_ptr, mvp, -0.30f, 0.22f, -1.30f, 0.30f, 0.30f, -0.40f,
                 hull_s, hull_t, hull_b);

    /* Engine nacelle pods — heavy */
    sfa_draw_box(fb_ptr, mvp, -0.58f, -0.10f, -1.40f, -0.42f, 0.14f, -0.60f,
                 hull_s, hull_b, hull_b);
    sfa_draw_box(fb_ptr, mvp,  0.42f, -0.10f, -1.40f,  0.58f, 0.14f, -0.60f,
                 hull_s, hull_b, hull_b);
    /* Nacelles — elongated aft behind engine pods */
    sfa_draw_box(fb_ptr, mvp, -0.56f, -0.06f, -2.00f, -0.44f, 0.10f, -0.60f,
                 0xFF2244AA, 0xFF1A3388, 0xFF112266);
    sfa_draw_box(fb_ptr, mvp,  0.44f, -0.06f, -2.00f,  0.56f, 0.10f, -0.60f,
                 0xFF2244AA, 0xFF1A3388, 0xFF112266);
    /* Engine glows */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.56f, -0.06f, -2.01f, 0,0, 0xFF2244AA),
        sr_vert_c(-0.56f,  0.10f, -2.01f, 0,1, 0xFF2244AA),
        sr_vert_c(-0.44f,  0.10f, -2.01f, 1,1, 0xFF2244AA),
        sr_vert_c(-0.44f, -0.06f, -2.01f, 1,0, 0xFF2244AA),
        NULL, mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c( 0.44f, -0.06f, -2.01f, 0,0, 0xFF2244AA),
        sr_vert_c( 0.44f,  0.10f, -2.01f, 0,1, 0xFF2244AA),
        sr_vert_c( 0.56f,  0.10f, -2.01f, 1,1, 0xFF2244AA),
        sr_vert_c( 0.56f, -0.06f, -2.01f, 1,0, 0xFF2244AA),
        NULL, mvp);

    /* Elongated forward section — tapered, integrated neck + head */
    sfa_draw_box(fb_ptr, mvp, -0.18f, -0.08f, -0.20f, 0.18f, 0.12f, 0.80f,
                 hull_t, hull_s, hull_b);
    /* Forward command section — wider */
    sfa_draw_box(fb_ptr, mvp, -0.28f, -0.12f, 0.80f, 0.28f, 0.16f, 1.50f,
                 head_t, head_s, hull_b);

    /* Heavy disruptor array */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.12f, -0.03f, 1.51f, 0,0, gun_col),
        sr_vert_c(-0.12f,  0.10f, 1.51f, 0,1, gun_col),
        sr_vert_c( 0.12f,  0.10f, 1.51f, 1,1, gun_col),
        sr_vert_c( 0.12f, -0.03f, 1.51f, 1,0, gun_col),
        NULL, mvp);

    /* Belly torpedo pod */
    sfa_draw_box(fb_ptr, mvp, -0.14f, -0.30f, -0.40f, 0.14f, -0.18f, 0.50f,
                 hull_s, hull_b, hull_b);
    /* Torpedo tube */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.06f, -0.28f, 0.51f, 0,0, gun_col),
        sr_vert_c(-0.06f, -0.20f, 0.51f, 0,1, gun_col),
        sr_vert_c( 0.06f, -0.20f, 0.51f, 1,1, gun_col),
        sr_vert_c( 0.06f, -0.28f, 0.51f, 1,0, gun_col),
        NULL, mvp);

    /* Flat horizontal wings — no sweep, no droop, nacelles at hull height */
    sfa_draw_klingon_wings(fb_ptr, mvp,
        0.42f, -1.50f, -0.20f,
        1.80f, 0.0f,
        -0.20f, -0.85f,
        0.10f,
        wing_t, wing_s, wing_b, gun_col);
}

/* ── Klingon ship dispatcher ────────────────────────────────────── */

/* Bounding box extents per Klingon class (for bracket sizing) */
static const float kling_bbox[SHIP_CLASS_COUNT][6] = {
    /* min_x,    min_y,   min_z,   max_x,  max_y,  max_z */
    { -0.79f, -0.10f, -0.45f,  0.79f, 0.10f, 0.51f },  /* FRIGATE */
    { -1.37f, -0.18f, -0.85f,  1.37f, 0.14f, 1.06f },  /* DESTROYER */
    { -1.58f, -0.24f, -1.10f,  1.58f, 0.18f, 1.31f },  /* CRUISER */
    { -1.90f, -0.18f, -2.01f,  1.90f, 0.30f, 1.51f },  /* BATTLECRUISER */
};

static void sfa_draw_target_ship(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                   float tx, float tz, float heading, int ship_class,
                                   bool is_boss) {
    int cls = ship_class;
    if (cls < 0 || cls >= SHIP_CLASS_COUNT) cls = SHIP_CLASS_DESTROYER;

    uint32_t hull_t, hull_s, hull_b, wing_t, wing_s, wing_b, head_t, head_s, gun_col;
    if (is_boss) {
        /* Boss ships: red/crimson color scheme */
        hull_t  = 0xFF3333CC; /* bright red (ABGR) */
        hull_s  = 0xFF2222AA;
        hull_b  = 0xFF111166;
        wing_t  = 0xFF4444DD;
        wing_s  = 0xFF3333AA;
        wing_b  = 0xFF222277;
        head_t  = 0xFF4455EE;
        head_s  = 0xFF3344CC;
        gun_col = 0xFF5566FF;
    } else {
        hull_t = sfa_pal_abgr(30);
        hull_s = sfa_pal_abgr(29);
        hull_b = sfa_pal_abgr(35);
        wing_t = sfa_pal_abgr(36);
        wing_s = sfa_pal_abgr(35);
        wing_b = sfa_pal_abgr(34);
        head_t = sfa_pal_abgr(31);
        head_s = sfa_pal_abgr(30);
        gun_col = sfa_pal_abgr(15);
    }

    sr_mat4 model = sr_mat4_mul(
        sr_mat4_translate(tx, 0.0f, tz),
        sr_mat4_rotate_y(-heading)
    );
    sr_mat4 mvp = sr_mat4_mul(*vp, model);

    switch (cls) {
        case SHIP_CLASS_FRIGATE:
            sfa_draw_kling_frigate(fb_ptr, &mvp, hull_t, hull_s, hull_b,
                                    wing_t, wing_s, wing_b, head_t, head_s, gun_col);
            break;
        case SHIP_CLASS_DESTROYER:
            sfa_draw_kling_destroyer(fb_ptr, &mvp, hull_t, hull_s, hull_b,
                                      wing_t, wing_s, wing_b, head_t, head_s, gun_col);
            break;
        case SHIP_CLASS_CRUISER:
            sfa_draw_kling_cruiser(fb_ptr, &mvp, hull_t, hull_s, hull_b,
                                    wing_t, wing_s, wing_b, head_t, head_s, gun_col);
            break;
        case SHIP_CLASS_BATTLECRUISER:
            sfa_draw_kling_battlecruiser(fb_ptr, &mvp, hull_t, hull_s, hull_b,
                                          wing_t, wing_s, wing_b, head_t, head_s, gun_col);
            break;
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
    /* Build bounding box from per-class extents */
    int cls = ship_class;
    if (cls < 0 || cls >= SHIP_CLASS_COUNT) cls = SHIP_CLASS_DESTROYER;
    const float *bb = kling_bbox[cls];
    float bbox_pts[8][3] = {
        {bb[0], bb[1], bb[2]}, {bb[3], bb[1], bb[2]},
        {bb[0], bb[4], bb[2]}, {bb[3], bb[4], bb[2]},
        {bb[0], bb[1], bb[5]}, {bb[3], bb[1], bb[5]},
        {bb[0], bb[4], bb[5]}, {bb[3], bb[4], bb[5]},
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
