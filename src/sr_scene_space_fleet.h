/*  sr_scene_space_fleet.h — Space Fleet Assault: tactical starship combat.
 *  Single-TU header-only. Depends on sr_app.h, sr_raster.h, sr_font.h. */
#ifndef SR_SCENE_SPACE_FLEET_H
#define SR_SCENE_SPACE_FLEET_H

#include <math.h>
#include <stdbool.h>

/* ── Constants ───────────────────────────────────────────────────── */

#define SFA_PI           3.14159265f
#define SFA_TWO_PI       6.28318530f
#define SFA_DEG2RAD      (SFA_PI / 180.0f)

#define SFA_ARENA_SIZE   80.0f      /* half-extent of playable area */
#define SFA_GRID_SPACING 5.0f       /* starfield grid spacing */

/* Speed levels (impulse) */
#define SFA_SPEED_STOP   0
#define SFA_SPEED_QUARTER 1
#define SFA_SPEED_HALF   2
#define SFA_SPEED_FULL   3
#define SFA_NUM_SPEEDS   4

static const float sfa_speed_values[SFA_NUM_SPEEDS] = {
    0.0f, 3.0f, 6.0f, 12.0f
};
static const char *sfa_speed_names[SFA_NUM_SPEEDS] = {
    "ALL STOP", "1/4 IMPULSE", "1/2 IMPULSE", "FULL IMPULSE"
};

/* Ship turn rate (radians/sec) */
#define SFA_TURN_RATE    2.0f

/* Camera */
#define SFA_CAM_HEIGHT   40.0f
#define SFA_CAM_TILT     (70.0f * SFA_DEG2RAD)  /* near top-down */

/* Colors (0xAABBGGRR) */
#define SFA_BG_COLOR       0xFF1A0A05   /* deep space dark */
#define SFA_GRID_COLOR     0xFF332211   /* faint star grid */
#define SFA_SHIP_COLOR     0xFFFF9933   /* player ship hull */
#define SFA_SHIP_ACCENT    0xFFFFCC66   /* nacelle glow */
#define SFA_ENGINE_COLOR   0xFFFF6622   /* engine exhaust */
#define SFA_SHIELD_COLOR   0xFF88DDFF   /* shield indicator */
#define SFA_HUD_BG         0xC0000000
#define SFA_HUD_TEXT       0xFFCCCCCC
#define SFA_HUD_BRIGHT     0xFFFFFFFF
#define SFA_HUD_ACCENT     0xFF55CCFF
#define SFA_HUD_WARN       0xFF4466FF
#define SFA_HUD_SHADOW     0xFF000000

/* ── Ship state ──────────────────────────────────────────────────── */

typedef struct {
    float x, z;               /* world position */
    float heading;            /* current heading in radians (0 = +Z, CW) */
    float target_heading;     /* desired heading */
    int   speed_level;        /* 0-3 */

    /* Shield facings: F, FR, AR, A, AL, FL (clockwise from front) */
    float shields[6];
    float hull;

    /* Smooth interpolation */
    float visual_heading;     /* smoothed heading for rendering */
} sfa_ship;

typedef struct {
    sfa_ship player;
    float    time;
    bool     initialized;

    /* Touch controls */
    bool     touch_steering;      /* is user dragging to steer? */
    float    touch_steer_cx;      /* center of steering circle (fb coords) */
    float    touch_steer_cy;
    float    touch_steer_angle;   /* current angle from touch */
    bool     touch_throttle;      /* throttle touch active */
} sfa_state;

static sfa_state sfa;

/* ── Key held state (for smooth turning) ─────────────────────────── */

static bool sfa_key_left  = false;
static bool sfa_key_right = false;
static bool sfa_key_up    = false;
static bool sfa_key_down  = false;

/* ── Initialization ──────────────────────────────────────────────── */

static void sfa_init(void) {
    memset(&sfa, 0, sizeof(sfa));

    sfa.player.x = 0.0f;
    sfa.player.z = 0.0f;
    sfa.player.heading = 0.0f;
    sfa.player.target_heading = 0.0f;
    sfa.player.visual_heading = 0.0f;
    sfa.player.speed_level = SFA_SPEED_STOP;
    sfa.player.hull = 100.0f;

    for (int i = 0; i < 6; i++)
        sfa.player.shields[i] = 100.0f;

    sfa.initialized = true;
}

/* ── Normalize angle to [-PI, PI] ────────────────────────────────── */

static float sfa_normalize_angle(float a) {
    while (a >  SFA_PI) a -= SFA_TWO_PI;
    while (a < -SFA_PI) a += SFA_TWO_PI;
    return a;
}

/* ── Update ──────────────────────────────────────────────────────── */

static void sfa_update(float dt) {
    sfa_ship *s = &sfa.player;
    sfa.time += dt;

    /* Apply continuous keyboard steering */
    if (sfa_key_left)  s->target_heading -= SFA_TURN_RATE * dt;
    if (sfa_key_right) s->target_heading += SFA_TURN_RATE * dt;

    /* Normalize target heading */
    s->target_heading = sfa_normalize_angle(s->target_heading);

    /* Rotate toward target heading */
    float diff = sfa_normalize_angle(s->target_heading - s->heading);
    float max_turn = SFA_TURN_RATE * dt;
    if (diff > max_turn) diff = max_turn;
    else if (diff < -max_turn) diff = -max_turn;
    s->heading += diff;
    s->heading = sfa_normalize_angle(s->heading);

    /* Smooth visual heading */
    float vdiff = sfa_normalize_angle(s->heading - s->visual_heading);
    s->visual_heading += vdiff * 8.0f * dt;
    s->visual_heading = sfa_normalize_angle(s->visual_heading);

    /* Move ship */
    float speed = sfa_speed_values[s->speed_level];
    s->x += sinf(s->heading) * speed * dt;
    s->z += cosf(s->heading) * speed * dt;

    /* Clamp to arena bounds */
    if (s->x >  SFA_ARENA_SIZE) s->x =  SFA_ARENA_SIZE;
    if (s->x < -SFA_ARENA_SIZE) s->x = -SFA_ARENA_SIZE;
    if (s->z >  SFA_ARENA_SIZE) s->z =  SFA_ARENA_SIZE;
    if (s->z < -SFA_ARENA_SIZE) s->z = -SFA_ARENA_SIZE;
}

/* ── Ship model (simple arrow/wedge shape) ───────────────────────── */

static void sfa_draw_ship(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                           sfa_ship *s) {
    /* Model: ship facing +Z, centered at origin */
    float h = s->visual_heading;

    sr_mat4 model = sr_mat4_mul(
        sr_mat4_translate(s->x, 0.0f, s->z),
        sr_mat4_rotate_y(h)
    );
    sr_mat4 mvp = sr_mat4_mul(*vp, model);

    /* Ship body — a pointed wedge (top-down: triangle nose + rect body) */
    /* Hull dimensions */
    float len = 1.5f;   /* half-length */
    float wid = 0.5f;   /* half-width */
    float nacelle_w = 0.15f;
    float y = 0.0f;     /* flat on the XZ plane */

    /* Main hull - forward triangle (nose) */
    sr_draw_triangle(fb_ptr,
        sr_vert_c( 0.0f, y,  len,      0.5f, 0, SFA_SHIP_COLOR),
        sr_vert_c(-wid,  y,  0.0f,     0,    1, SFA_SHIP_COLOR),
        sr_vert_c( wid,  y,  0.0f,     1,    1, SFA_SHIP_COLOR),
        NULL, &mvp);

    /* Main hull - aft rectangle */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-wid, y,  0.0f,    0, 0, SFA_SHIP_COLOR),
        sr_vert_c(-wid, y, -len*0.6f, 0, 1, SFA_SHIP_COLOR),
        sr_vert_c( wid, y, -len*0.6f, 1, 1, SFA_SHIP_COLOR),
        sr_vert_c( wid, y,  0.0f,    1, 0, SFA_SHIP_COLOR),
        NULL, &mvp);

    /* Left nacelle */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-wid - nacelle_w, y, -0.2f,   0, 0, SFA_SHIP_ACCENT),
        sr_vert_c(-wid - nacelle_w, y, -len*0.8f, 0, 1, SFA_SHIP_ACCENT),
        sr_vert_c(-wid,             y, -len*0.8f, 1, 1, SFA_SHIP_ACCENT),
        sr_vert_c(-wid,             y, -0.2f,   1, 0, SFA_SHIP_ACCENT),
        NULL, &mvp);

    /* Right nacelle */
    sr_draw_quad(fb_ptr,
        sr_vert_c( wid,             y, -0.2f,   0, 0, SFA_SHIP_ACCENT),
        sr_vert_c( wid,             y, -len*0.8f, 0, 1, SFA_SHIP_ACCENT),
        sr_vert_c( wid + nacelle_w, y, -len*0.8f, 1, 1, SFA_SHIP_ACCENT),
        sr_vert_c( wid + nacelle_w, y, -0.2f,   1, 0, SFA_SHIP_ACCENT),
        NULL, &mvp);

    /* Engine glow (visible when moving) */
    if (s->speed_level > 0) {
        float glow_len = 0.3f + 0.2f * s->speed_level;
        float pulse = 0.7f + 0.3f * sinf(sfa.time * 12.0f);
        uint8_t gr = (uint8_t)(255.0f * pulse);
        uint8_t gg = (uint8_t)(100.0f * pulse);
        uint32_t glow_col = 0xFF000000 | (uint32_t)(0x22) << 16 | (uint32_t)gg << 8 | gr;

        /* Center engine */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(0.0f,  y, -len*0.6f,          0.5f, 0, glow_col),
            sr_vert_c(-0.15f, y, -len*0.6f - glow_len, 0, 1, 0xFF000000),
            sr_vert_c( 0.15f, y, -len*0.6f - glow_len, 1, 1, 0xFF000000),
            NULL, &mvp);

        /* Nacelle engines */
        float nx = wid + nacelle_w * 0.5f;
        sr_draw_triangle(fb_ptr,
            sr_vert_c(-nx,       y, -len*0.8f,           0.5f, 0, glow_col),
            sr_vert_c(-nx-0.08f, y, -len*0.8f - glow_len*0.7f, 0, 1, 0xFF000000),
            sr_vert_c(-nx+0.08f, y, -len*0.8f - glow_len*0.7f, 1, 1, 0xFF000000),
            NULL, &mvp);
        sr_draw_triangle(fb_ptr,
            sr_vert_c( nx,       y, -len*0.8f,           0.5f, 0, glow_col),
            sr_vert_c( nx-0.08f, y, -len*0.8f - glow_len*0.7f, 0, 1, 0xFF000000),
            sr_vert_c( nx+0.08f, y, -len*0.8f - glow_len*0.7f, 1, 1, 0xFF000000),
            NULL, &mvp);
    }
}

/* ── Starfield background (flat grid on XZ plane) ────────────────── */

static void sfa_draw_starfield(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                 float cam_x, float cam_z) {
    /* Draw scattered star points as tiny quads on the Y=0 plane */
    float star_size = 0.08f;

    /* Use a grid of "stars" — deterministic based on position */
    float spacing = SFA_GRID_SPACING;
    float view_range = 50.0f;
    float x_start = floorf((cam_x - view_range) / spacing) * spacing;
    float z_start = floorf((cam_z - view_range) / spacing) * spacing;

    for (float gx = x_start; gx < cam_x + view_range; gx += spacing) {
        for (float gz = z_start; gz < cam_z + view_range; gz += spacing) {
            /* Deterministic pseudo-random offset per grid cell */
            int ix = (int)floorf(gx / spacing);
            int iz = (int)floorf(gz / spacing);
            uint32_t seed = (uint32_t)(ix * 73856093u ^ iz * 19349663u);

            /* 2-3 stars per cell */
            int n_stars = 1 + (seed % 3);
            for (int si = 0; si < n_stars; si++) {
                seed = seed * 1103515245u + 12345u + si * 7u;
                float ox = (float)((seed >> 4) & 0xFF) / 255.0f * spacing;
                seed = seed * 1103515245u + 12345u;
                float oz = (float)((seed >> 4) & 0xFF) / 255.0f * spacing;
                seed = seed * 1103515245u + 12345u;
                float brightness = 0.15f + (float)((seed >> 4) & 0xFF) / 255.0f * 0.5f;

                float sx = gx + ox;
                float sz = gz + oz;
                float ss = star_size * (0.5f + brightness);

                uint8_t bv = (uint8_t)(brightness * 255.0f);
                uint32_t col = 0xFF000000 | ((uint32_t)bv << 16) | ((uint32_t)bv << 8) | bv;

                /* Tiny quad on the plane */
                float y = -0.1f;
                sr_draw_quad(fb_ptr,
                    sr_vert_c(sx - ss, y, sz - ss, 0, 0, col),
                    sr_vert_c(sx - ss, y, sz + ss, 0, 1, col),
                    sr_vert_c(sx + ss, y, sz + ss, 1, 1, col),
                    sr_vert_c(sx + ss, y, sz - ss, 1, 0, col),
                    NULL, vp);
            }
        }
    }
}

/* ── Arena boundary markers ──────────────────────────────────────── */

static void sfa_draw_arena_boundary(sr_framebuffer *fb_ptr, const sr_mat4 *vp) {
    float s = SFA_ARENA_SIZE;
    float y = -0.05f;
    float w = 0.15f;  /* line width */
    uint32_t col = 0xFF222255;  /* dim red border */

    /* Four edges */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-s, y, -s,   0,0, col), sr_vert_c(-s, y, s,    0,1, col),
        sr_vert_c(-s+w, y, s,  1,1, col), sr_vert_c(-s+w, y, -s, 1,0, col),
        NULL, vp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(s-w, y, -s,  0,0, col), sr_vert_c(s-w, y, s,   0,1, col),
        sr_vert_c(s, y, s,     1,1, col), sr_vert_c(s, y, -s,    1,0, col),
        NULL, vp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(-s, y, -s,   0,0, col), sr_vert_c(-s, y, -s+w, 0,1, col),
        sr_vert_c(s, y, -s+w,  1,1, col), sr_vert_c(s, y, -s,    1,0, col),
        NULL, vp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(-s, y, s-w,  0,0, col), sr_vert_c(-s, y, s,    0,1, col),
        sr_vert_c(s, y, s,     1,1, col), sr_vert_c(s, y, s-w,   1,0, col),
        NULL, vp);
}

/* ── HUD Drawing Helpers ─────────────────────────────────────────── */

static void sfa_draw_rect(uint32_t *px, int W, int H,
                           int x0, int y0, int x1, int y1, uint32_t col) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    for (int ry = y0; ry < y1; ry++)
        for (int rx = x0; rx < x1; rx++)
            px[ry * W + rx] = col;
}

/* Draw a filled circle (for touch controls) */
static void sfa_draw_circle(uint32_t *px, int W, int H,
                              int cx, int cy, int radius, uint32_t col) {
    int r2 = radius * radius;
    int x0 = cx - radius, x1 = cx + radius;
    int y0 = cy - radius, y1 = cy + radius;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= W) x1 = W - 1;
    if (y1 >= H) y1 = H - 1;
    for (int ry = y0; ry <= y1; ry++)
        for (int rx = x0; rx <= x1; rx++) {
            int dx = rx - cx, dy = ry - cy;
            if (dx*dx + dy*dy <= r2)
                px[ry * W + rx] = col;
        }
}

/* Draw a ring (circle outline) */
static void sfa_draw_ring(uint32_t *px, int W, int H,
                            int cx, int cy, int radius, int thickness, uint32_t col) {
    int outer2 = radius * radius;
    int inner = radius - thickness;
    int inner2 = inner * inner;
    int x0 = cx - radius, x1 = cx + radius;
    int y0 = cy - radius, y1 = cy + radius;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= W) x1 = W - 1;
    if (y1 >= H) y1 = H - 1;
    for (int ry = y0; ry <= y1; ry++)
        for (int rx = x0; rx <= x1; rx++) {
            int dx = rx - cx, dy = ry - cy;
            int d2 = dx*dx + dy*dy;
            if (d2 <= outer2 && d2 >= inner2)
                px[ry * W + rx] = col;
        }
}

/* ── HUD: Shield display (hexagonal around miniship) ─────────────── */

static void sfa_draw_shield_hud(uint32_t *px, int W, int H,
                                  sfa_ship *s, int cx, int cy) {
    /* Draw 6 shield arc indicators around center */
    int r = 18;
    int thickness = 3;

    /* Shield arc angles: F, FR, AR, A, AL, FL */
    float arc_starts[6] = {
        -30.0f, 30.0f, 90.0f, 150.0f, -150.0f, -90.0f
    };

    for (int i = 0; i < 6; i++) {
        float pct = s->shields[i] / 100.0f;
        uint8_t g = (uint8_t)(pct * 255.0f);
        uint8_t red = (uint8_t)((1.0f - pct) * 255.0f);
        uint32_t col = 0xC0000000 | ((uint32_t)0x00 << 16) | ((uint32_t)g << 8) | red;

        float a0 = (arc_starts[i]) * SFA_DEG2RAD;
        float a1 = (arc_starts[i] + 60.0f) * SFA_DEG2RAD;

        /* Draw arc as pixels */
        for (int dy = -r - thickness; dy <= r + thickness; dy++) {
            for (int dx = -r - thickness; dx <= r + thickness; dx++) {
                int d2 = dx*dx + dy*dy;
                int outer2 = (r + thickness) * (r + thickness);
                int inner2 = r * r;
                if (d2 > outer2 || d2 < inner2) continue;

                float angle = atan2f((float)dx, (float)-dy);  /* 0=up, CW */
                float na = sfa_normalize_angle(angle - a0);
                float span = sfa_normalize_angle(a1 - a0);
                if (span < 0) span += SFA_TWO_PI;
                if (na < 0) na += SFA_TWO_PI;
                if (na <= span) {
                    int px2 = cx + dx, py2 = cy + dy;
                    if (px2 >= 0 && px2 < W && py2 >= 0 && py2 < H)
                        px[py2 * W + px2] = col;
                }
            }
        }
    }

    /* Mini ship indicator in center */
    /* Tiny triangle pointing up (forward) */
    for (int dy = -6; dy <= 6; dy++) {
        int hw = (6 - (dy < 0 ? -dy : dy)) / 2;
        for (int dx = -hw; dx <= hw; dx++) {
            int px2 = cx + dx, py2 = cy + dy;
            if (px2 >= 0 && px2 < W && py2 >= 0 && py2 < H)
                px[py2 * W + px2] = SFA_SHIP_COLOR;
        }
    }
}

/* ── HUD: Speed bar ──────────────────────────────────────────────── */

static void sfa_draw_speed_hud(uint32_t *px, int W, int H,
                                 sfa_ship *s, int x, int y) {
    /* Speed label */
    sr_draw_text_shadow(px, W, H, x, y, sfa_speed_names[s->speed_level],
                         SFA_HUD_ACCENT, SFA_HUD_SHADOW);

    /* Speed bar */
    int bar_y = y + 10;
    int bar_w = 60;
    int bar_h = 5;
    sfa_draw_rect(px, W, H, x, bar_y, x + bar_w, bar_y + bar_h, SFA_HUD_BG);

    int fill_w = (bar_w * s->speed_level) / (SFA_NUM_SPEEDS - 1);
    if (fill_w > 0) {
        uint32_t fill_col = s->speed_level == SFA_SPEED_FULL ? SFA_HUD_WARN : SFA_HUD_ACCENT;
        sfa_draw_rect(px, W, H, x, bar_y, x + fill_w, bar_y + bar_h, fill_col);
    }
}

/* ── HUD: Heading compass ────────────────────────────────────────── */

static void sfa_draw_heading_hud(uint32_t *px, int W, int H,
                                   sfa_ship *s, int cx, int y) {
    char buf[16];
    int deg = (int)(s->heading * 180.0f / SFA_PI);
    if (deg < 0) deg += 360;
    snprintf(buf, sizeof(buf), "HDG %03d", deg);
    int text_w = 7 * 6;  /* approx */
    sr_draw_text_shadow(px, W, H, cx - text_w / 2, y, buf,
                         SFA_HUD_BRIGHT, SFA_HUD_SHADOW);
}

/* ── HUD: Mobile virtual controls ────────────────────────────────── */

/* Virtual steering wheel (left side) + throttle buttons (right side) */

#define SFA_VCTRL_STEER_R   30    /* steering circle radius in fb pixels */
#define SFA_VCTRL_STEER_CX  45    /* center X of steering circle */
#define SFA_VCTRL_STEER_CY  (FB_HEIGHT - 50)

#define SFA_VCTRL_BTN_W     36
#define SFA_VCTRL_BTN_H     18
#define SFA_VCTRL_BTN_X     (FB_WIDTH - SFA_VCTRL_BTN_W - 6)
#define SFA_VCTRL_BTN_GAP   4

static void sfa_draw_mobile_controls(uint32_t *px, int W, int H, sfa_ship *s) {
    /* ── Left side: steering circle ── */
    int scx = SFA_VCTRL_STEER_CX;
    int scy = SFA_VCTRL_STEER_CY;
    int sr_radius = SFA_VCTRL_STEER_R;

    /* Outer ring */
    sfa_draw_ring(px, W, H, scx, scy, sr_radius, 2, 0x60FFFFFF);

    /* Direction indicator — small dot showing current heading */
    float vis_h = s->visual_heading;
    int dot_x = scx + (int)(sinf(vis_h) * (sr_radius - 6));
    int dot_y = scy - (int)(cosf(vis_h) * (sr_radius - 6));
    sfa_draw_circle(px, W, H, dot_x, dot_y, 3, SFA_HUD_ACCENT);

    /* If steering active, show target heading */
    if (sfa.touch_steering) {
        int tgt_x = scx + (int)(sinf(sfa.touch_steer_angle) * (sr_radius - 6));
        int tgt_y = scy - (int)(cosf(sfa.touch_steer_angle) * (sr_radius - 6));
        sfa_draw_circle(px, W, H, tgt_x, tgt_y, 2, SFA_HUD_WARN);
    }

    /* Center dot */
    sfa_draw_circle(px, W, H, scx, scy, 2, 0x40FFFFFF);

    /* Label */
    sr_draw_text_shadow(px, W, H, scx - 12, scy - sr_radius - 10,
                         "HELM", SFA_HUD_TEXT, SFA_HUD_SHADOW);

    /* ── Right side: throttle buttons ── */
    int bx = SFA_VCTRL_BTN_X;
    int by_base = H - 10 - (SFA_NUM_SPEEDS * (SFA_VCTRL_BTN_H + SFA_VCTRL_BTN_GAP));

    sr_draw_text_shadow(px, W, H, bx + 2, by_base - 10,
                         "SPEED", SFA_HUD_TEXT, SFA_HUD_SHADOW);

    for (int i = SFA_NUM_SPEEDS - 1; i >= 0; i--) {
        int idx = SFA_NUM_SPEEDS - 1 - i;  /* draw highest at top */
        int by = by_base + idx * (SFA_VCTRL_BTN_H + SFA_VCTRL_BTN_GAP);

        uint32_t bg = (i == s->speed_level) ? 0xC0332200 : SFA_HUD_BG;
        uint32_t fg = (i == s->speed_level) ? SFA_HUD_ACCENT : SFA_HUD_TEXT;

        sfa_draw_rect(px, W, H, bx, by, bx + SFA_VCTRL_BTN_W, by + SFA_VCTRL_BTN_H, bg);

        char label[8];
        if (i == 0) snprintf(label, sizeof(label), "STOP");
        else snprintf(label, sizeof(label), "%d/4", i);
        sr_draw_text_shadow(px, W, H, bx + 4, by + 5, label, fg, SFA_HUD_SHADOW);
    }
}

/* ── HUD: top bar ────────────────────────────────────────────────── */

static void sfa_draw_hud(sr_framebuffer *fb_ptr, sfa_ship *s) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    /* Top bar background */
    sfa_draw_rect(px, W, H, 0, 0, W, 14, SFA_HUD_BG);

    /* Heading */
    sfa_draw_heading_hud(px, W, H, s, W / 2, 3);

    /* Speed indicator (top-left) */
    sfa_draw_speed_hud(px, W, H, s, 3, 3);

    /* Shield + hull (top-right area) */
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "HULL: %d%%", (int)s->hull);
        int tw = (int)strlen(buf) * 6;
        sr_draw_text_shadow(px, W, H, W - tw - 4, 3, buf,
                             s->hull > 50 ? SFA_HUD_BRIGHT : SFA_HUD_WARN, SFA_HUD_SHADOW);
    }

    /* Shield hex display (bottom-left) */
    sfa_draw_shield_hud(px, W, H, s, 45, H - 100);

    /* Mobile virtual controls */
    sfa_draw_mobile_controls(px, W, H, s);

    /* MENU button (top-right) */
    {
        int mbx = W - 32, mby = 3, mbw = 30, mbh = 11;
        sfa_draw_rect(px, W, H, mbx, mby, mbx + mbw, mby + mbh, SFA_HUD_BG);
        sr_draw_text_shadow(px, W, H, mbx + 3, mby + 2, "MENU",
                             0xFF999999, SFA_HUD_SHADOW);
    }
}

/* ── Touch input handling ────────────────────────────────────────── */

static bool sfa_handle_touch_began(float sx, float sy) {
    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);

    /* Check MENU button */
    int mbx = FB_WIDTH - 32, mby = 3, mbw = 30, mbh = 11;
    if (fx >= mbx && fx <= mbx + mbw && fy >= mby && fy <= mby + mbh) {
        app_state = STATE_MENU;
        return true;
    }

    /* Check steering circle */
    float sdx = fx - SFA_VCTRL_STEER_CX;
    float sdy = fy - SFA_VCTRL_STEER_CY;
    float sdist = sqrtf(sdx * sdx + sdy * sdy);
    if (sdist < SFA_VCTRL_STEER_R + 15) {
        sfa.touch_steering = true;
        sfa.touch_steer_cx = SFA_VCTRL_STEER_CX;
        sfa.touch_steer_cy = SFA_VCTRL_STEER_CY;
        if (sdist > 5.0f) {
            sfa.touch_steer_angle = atan2f(sdx, -sdy);
            sfa.player.target_heading = sfa.touch_steer_angle;
        }
        return true;
    }

    /* Check throttle buttons */
    int bx = SFA_VCTRL_BTN_X;
    int by_base = FB_HEIGHT - 10 - (SFA_NUM_SPEEDS * (SFA_VCTRL_BTN_H + SFA_VCTRL_BTN_GAP));
    for (int i = SFA_NUM_SPEEDS - 1; i >= 0; i--) {
        int idx = SFA_NUM_SPEEDS - 1 - i;
        int by = by_base + idx * (SFA_VCTRL_BTN_H + SFA_VCTRL_BTN_GAP);
        if (fx >= bx && fx <= bx + SFA_VCTRL_BTN_W &&
            fy >= by && fy <= by + SFA_VCTRL_BTN_H) {
            sfa.player.speed_level = i;
            return true;
        }
    }

    return false;
}

static void sfa_handle_touch_moved(float sx, float sy) {
    if (!sfa.touch_steering) return;

    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);

    float sdx = fx - sfa.touch_steer_cx;
    float sdy = fy - sfa.touch_steer_cy;
    float sdist = sqrtf(sdx * sdx + sdy * sdy);

    if (sdist > 5.0f) {
        sfa.touch_steer_angle = atan2f(sdx, -sdy);
        sfa.player.target_heading = sfa.touch_steer_angle;
    }
}

static void sfa_handle_touch_ended(void) {
    sfa.touch_steering = false;
    sfa.touch_throttle = false;
}

/* ── Key input ───────────────────────────────────────────────────── */

static void sfa_handle_key_down(sapp_keycode key) {
    sfa_ship *s = &sfa.player;

    switch (key) {
        case SAPP_KEYCODE_LEFT:
        case SAPP_KEYCODE_A:
            sfa_key_left = true;
            break;
        case SAPP_KEYCODE_RIGHT:
        case SAPP_KEYCODE_D:
            sfa_key_right = true;
            break;
        case SAPP_KEYCODE_UP:
        case SAPP_KEYCODE_W:
            if (s->speed_level < SFA_NUM_SPEEDS - 1)
                s->speed_level++;
            break;
        case SAPP_KEYCODE_DOWN:
        case SAPP_KEYCODE_S:
            if (s->speed_level > 0)
                s->speed_level--;
            break;
        default:
            break;
    }
}

static void sfa_handle_key_up(sapp_keycode key) {
    switch (key) {
        case SAPP_KEYCODE_LEFT:
        case SAPP_KEYCODE_A:
            sfa_key_left = false;
            break;
        case SAPP_KEYCODE_RIGHT:
        case SAPP_KEYCODE_D:
            sfa_key_right = false;
            break;
        default:
            break;
    }
}

/* ── Main scene draw ─────────────────────────────────────────────── */

static void draw_space_fleet_scene(sr_framebuffer *fb_ptr, float dt) {
    if (!sfa.initialized) sfa_init();

    sfa_update(dt);

    sfa_ship *s = &sfa.player;

    /* ── Top-down camera following the ship ── */
    sr_vec3 eye = {
        s->x,
        SFA_CAM_HEIGHT,
        s->z - 8.0f    /* slight offset behind for perspective */
    };
    sr_vec3 target = { s->x, 0.0f, s->z };
    sr_vec3 up = { 0, 0, 1 };  /* Z-up for top-down feel */

    sr_mat4 view = sr_mat4_lookat(eye, target, up);
    sr_mat4 proj = sr_mat4_perspective(
        30.0f * SFA_DEG2RAD,
        (float)FB_WIDTH / (float)FB_HEIGHT,
        1.0f, 200.0f
    );
    sr_mat4 vp = sr_mat4_mul(proj, view);

    /* Draw world */
    sfa_draw_starfield(fb_ptr, &vp, s->x, s->z);
    sfa_draw_arena_boundary(fb_ptr, &vp);

    /* Draw ship */
    sfa_draw_ship(fb_ptr, &vp, s);

    /* Draw HUD */
    sfa_draw_hud(fb_ptr, s);
}

#endif /* SR_SCENE_SPACE_FLEET_H */
