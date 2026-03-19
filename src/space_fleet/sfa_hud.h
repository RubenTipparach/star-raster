/*  sfa_hud.h — HUD drawing: helpers, shields, mobile controls, weapon bars, top bar. Header-only. Depends on sfa_types.h, sfa_render.h. */
#ifndef SFA_HUD_H
#define SFA_HUD_H

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

/* ── Targeting brackets (HUD overlay in screen space) ────────────── */

static void sfa_draw_bracket_corner(uint32_t *px, int W, int H,
                                      int cx, int cy, int size,
                                      int dx, int dy, uint32_t col) {
    /* Draw an L-shaped corner bracket */
    int arm = size / 3;
    int thick = 1;
    /* Horizontal arm */
    int hx0 = cx, hx1 = cx + dx * arm;
    if (hx0 > hx1) { int t = hx0; hx0 = hx1; hx1 = t; }
    int hy0 = cy, hy1 = cy + dy * thick;
    if (hy0 > hy1) { int t = hy0; hy0 = hy1; hy1 = t; }
    sfa_draw_rect(px, W, H, hx0, hy0, hx1 + 1, hy1 + 1, col);
    /* Vertical arm */
    int vx0 = cx, vx1 = cx + dx * thick;
    if (vx0 > vx1) { int t = vx0; vx0 = vx1; vx1 = t; }
    int vy0 = cy, vy1 = cy + dy * arm;
    if (vy0 > vy1) { int t = vy0; vy0 = vy1; vy1 = t; }
    sfa_draw_rect(px, W, H, vx0, vy0, vx1 + 1, vy1 + 1, col);
}

static void sfa_draw_targeting_brackets(uint32_t *px, int W, int H,
                                          int cx, int cy, int half_size,
                                          uint32_t col) {
    /* Four corner brackets */
    sfa_draw_bracket_corner(px, W, H, cx - half_size, cy - half_size, half_size*2,  1,  1, col);
    sfa_draw_bracket_corner(px, W, H, cx + half_size, cy - half_size, half_size*2, -1,  1, col);
    sfa_draw_bracket_corner(px, W, H, cx - half_size, cy + half_size, half_size*2,  1, -1, col);
    sfa_draw_bracket_corner(px, W, H, cx + half_size, cy + half_size, half_size*2, -1, -1, col);
}

/* ── Hover detection: check if mouse is near a projected NPC ─────── */

static void sfa_update_hover(const sr_mat4 *vp, int fb_w, int fb_h) {
    sfa.hovered_npc = -1;
    float best_dist = 1e9f;

    for (int i = 0; i < sfa.npc_count; i++) {
        sfa_ship *npc = &sfa.npcs[i];
        int scr_x, scr_y;
        float scr_w;
        if (!sfa_project_to_screen(vp, npc->x, 0.2f, npc->z, fb_w, fb_h, &scr_x, &scr_y, &scr_w))
            continue;

        /* Compute bracket size from projected ship bounding box */
        int bracket_half = sfa_ship_screen_extent(vp, npc->x, npc->z,
                                                    npc->visual_heading, fb_w, fb_h,
                                                    npc->ship_class);

        /* Check if mouse is inside the bracket area */
        float ddx = sfa.mouse_fb_x - (float)scr_x;
        float ddy = sfa.mouse_fb_y - (float)scr_y;
        float pixel_dist = sqrtf(ddx*ddx + ddy*ddy);
        if (pixel_dist <= (float)bracket_half && pixel_dist < best_dist) {
            best_dist = pixel_dist;
            sfa.hovered_npc = i;
        }
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

    /* Speed bar — target speed (dim) with actual speed overlay (bright) */
    int bar_y = y + 10;
    int bar_w = 60;
    int bar_h = 5;
    float max_speed = sfa_speed_values[SFA_NUM_SPEEDS - 1];
    sfa_draw_rect(px, W, H, x, bar_y, x + bar_w, bar_y + bar_h, SFA_HUD_BG);

    /* Target speed (dim background bar) */
    float target_speed = sfa_speed_values[s->speed_level];
    int target_w = (max_speed > 0) ? (int)(bar_w * target_speed / max_speed) : 0;
    if (target_w > 0) {
        uint32_t dim_col = 0x60335577;
        sfa_draw_rect(px, W, H, x, bar_y, x + target_w, bar_y + bar_h, dim_col);
    }

    /* Actual speed (bright fill) */
    int fill_w = (max_speed > 0) ? (int)(bar_w * s->current_speed / max_speed) : 0;
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

/* Turn buttons (top-left and top-right of screen) */
#define SFA_TURN_BTN_W      40
#define SFA_TURN_BTN_H      30
#define SFA_TURN_BTN_Y      30    /* below top bar */
#define SFA_TURN_BTN_MARGIN 6

static void sfa_draw_mobile_controls(uint32_t *px, int W, int H, sfa_ship *s) {
    /* ── Left side: steering circle ── */
    int scx = SFA_VCTRL_STEER_CX;
    int scy = SFA_VCTRL_STEER_CY;
    int sr_radius = SFA_VCTRL_STEER_R;

    /* Outer ring */
    sfa_draw_ring(px, W, H, scx, scy, sr_radius, 2, 0x60FFFFFF);

    /* Target heading line — shown when ship is rotating */
    float vis_h = s->visual_heading;
    {
        float angle_diff = sfa_normalize_angle(s->target_heading - s->heading);
        if (angle_diff > 0.01f || angle_diff < -0.01f) {
            float th = s->target_heading;
            int steps = sr_radius - 6;
            uint32_t tgt_col = 0xFF00AAFF; /* orange target line */
            for (int i = 0; i < steps; i++) {
                float t = (float)i / (float)steps;
                int lx = scx + (int)(sinf(th) * t * (sr_radius - 6));
                int ly = scy - (int)(cosf(th) * t * (sr_radius - 6));
                if (lx >= 0 && lx < W && ly >= 0 && ly < H)
                    px[ly * W + lx] = tgt_col;
            }
            /* Target dot at end */
            int tgt_x = scx + (int)(sinf(th) * (sr_radius - 6));
            int tgt_y = scy - (int)(cosf(th) * (sr_radius - 6));
            sfa_draw_circle(px, W, H, tgt_x, tgt_y, 2, tgt_col);
        }
    }

    /* If touch steering active, show touch target too */
    if (sfa.touch_steering) {
        int tgt_x = scx + (int)(sinf(sfa.touch_steer_angle) * (sr_radius - 6));
        int tgt_y = scy - (int)(cosf(sfa.touch_steer_angle) * (sr_radius - 6));
        sfa_draw_circle(px, W, H, tgt_x, tgt_y, 2, SFA_HUD_WARN);
    }

    /* Blue arrow icon centered on dial, rotated with 2D rotation matrix */
    {
        /* Arrow shape in local space (pointing UP, centered at origin) */
        /* Chevron: nose, left wing, tail notch, right wing */
        float arrow_pts[][2] = {
            {  0.0f, -10.0f },  /* 0: nose (top) */
            { -6.0f,   4.0f },  /* 1: left wing */
            {  0.0f,   1.0f },  /* 2: tail notch */
            {  6.0f,   4.0f },  /* 3: right wing */
        };
        int npts = 4;

        /* 2D rotation matrix: rotate by vis_h */
        /* dial convention: +heading rotates clockwise on dial (sin,cos) */
        float c = cosf(vis_h), si = sinf(vis_h);

        /* Transform all points: x' = x*cos - y*sin, y' = x*sin + y*cos */
        int sx[4], sy[4];
        for (int i = 0; i < npts; i++) {
            float lx = arrow_pts[i][0], ly = arrow_pts[i][1];
            float rx = lx * c - ly * si;
            float ry = lx * si + ly * c;
            sx[i] = scx + (int)(rx);
            sy[i] = scy + (int)(ry);
        }

        uint32_t arrow_col = 0xFFFF8833; /* ABGR: blue */

        /* Fill triangle: nose(0), left(1), notch(2) */
        for (int i = 0; i <= 30; i++) {
            float t = (float)i / 30.0f;
            /* Edge nose→left */
            int ax = sx[0] + (int)((sx[1] - sx[0]) * t);
            int ay = sy[0] + (int)((sy[1] - sy[0]) * t);
            /* Edge nose→notch */
            int bx = sx[0] + (int)((sx[2] - sx[0]) * t);
            int by = sy[0] + (int)((sy[2] - sy[0]) * t);
            int len = (int)(sqrtf((float)((bx-ax)*(bx-ax)+(by-ay)*(by-ay)))) + 1;
            for (int j = 0; j <= len; j++) {
                float u = (len > 0) ? (float)j / (float)len : 0;
                int px2 = ax + (int)((bx - ax) * u);
                int py2 = ay + (int)((by - ay) * u);
                if (px2 >= 0 && px2 < W && py2 >= 0 && py2 < H)
                    px[py2 * W + px2] = arrow_col;
            }
        }
        /* Fill triangle: nose(0), notch(2), right(3) */
        for (int i = 0; i <= 30; i++) {
            float t = (float)i / 30.0f;
            int ax = sx[0] + (int)((sx[3] - sx[0]) * t);
            int ay = sy[0] + (int)((sy[3] - sy[0]) * t);
            int bx = sx[0] + (int)((sx[2] - sx[0]) * t);
            int by = sy[0] + (int)((sy[2] - sy[0]) * t);
            int len = (int)(sqrtf((float)((bx-ax)*(bx-ax)+(by-ay)*(by-ay)))) + 1;
            for (int j = 0; j <= len; j++) {
                float u = (len > 0) ? (float)j / (float)len : 0;
                int px2 = ax + (int)((bx - ax) * u);
                int py2 = ay + (int)((by - ay) * u);
                if (px2 >= 0 && px2 < W && py2 >= 0 && py2 < H)
                    px[py2 * W + px2] = arrow_col;
            }
        }
    }

    /* North (heading=0) indicator — yellow line, shown while rotating */
    {
        float angle_diff = sfa_normalize_angle(s->target_heading - s->heading);
        if (angle_diff > 0.01f || angle_diff < -0.01f) {
            /* Draw yellow "N" line at heading 0 on the dial (top = 0) */
            uint32_t yellow = 0xFF00FFFF; /* ABGR yellow */
            int n_end_x = scx;
            int n_end_y = scy - (sr_radius - 2);
            for (int i = 0; i < sr_radius - 6; i++) {
                float t = (float)i / (float)(sr_radius - 6);
                int lx = scx;
                int ly = scy - (int)(t * (sr_radius - 2));
                if (lx >= 0 && lx < W && ly >= 0 && ly < H)
                    px[ly * W + lx] = yellow;
            }
            sr_draw_text_shadow(px, W, H, n_end_x - 2, n_end_y - 8,
                                 "N", yellow, SFA_HUD_SHADOW);
        }
    }

    /* Enemy dots on minimap — plot world XZ relative to player (no rotation, dial is fixed) */
    {
        float max_range = sfa.long_range_sensors ? SFA_SENSOR_LONG : SFA_SENSOR_SHORT;
        float map_r = (float)(sr_radius - 4); /* usable pixel radius */
        for (int i = 0; i < sfa.npc_count; i++) {
            sfa_ship *npc = &sfa.npcs[i];
            if (!npc->alive) continue;
            float dx = npc->x - sfa.player.x;
            float dz = npc->z - sfa.player.z;
            float dist = sqrtf(dx * dx + dz * dz);

            /* Unit direction * scaled distance, clamped to minimap radius */
            float r = (dist < 0.01f) ? 0.0f : fminf(dist / max_range, 1.0f) * map_r;
            float ux = (dist < 0.01f) ? 0.0f : dx / dist;
            float uz = (dist < 0.01f) ? 0.0f : dz / dist;

            int ex = scx - (int)(ux * r);  /* minimap right = world -X (heading 90) */
            int ey = scy - (int)(uz * r);

            uint32_t dot_col = (i == sfa.selected_npc)
                ? 0xFFFF6464 : 0xFF4444CC;
            int dot_size = (i == sfa.selected_npc) ? 3 : 2;
            sfa_draw_circle(px, W, H, ex, ey, dot_size, dot_col);
        }
    }

    /* Center dot */
    sfa_draw_circle(px, W, H, scx, scy, 2, 0x40FFFFFF);

    /* Degree labels around dial: 0 (top), 90 (right), 180 (bottom), 270 (left) */
    {
        int lr = sr_radius + 5;
        sr_draw_text_shadow(px, W, H, scx - 3, scy - lr - 4, "0", 0xFF888888, SFA_HUD_SHADOW);
        sr_draw_text_shadow(px, W, H, scx + lr, scy - 3, "90", 0xFF888888, SFA_HUD_SHADOW);
        sr_draw_text_shadow(px, W, H, scx - 6, scy + lr - 2, "180", 0xFF888888, SFA_HUD_SHADOW);
        sr_draw_text_shadow(px, W, H, scx - lr - 12, scy - 3, "270", 0xFF888888, SFA_HUD_SHADOW);
    }

    /* Label */
    sr_draw_text_shadow(px, W, H, scx - 12, scy - sr_radius - 14,
                         "HELM", SFA_HUD_TEXT, SFA_HUD_SHADOW);

    /* ── Right side: throttle buttons ── */
    int bx = SFA_VCTRL_BTN_X;
    int btn_stack_h = SFA_NUM_SPEEDS * (SFA_VCTRL_BTN_H + SFA_VCTRL_BTN_GAP);
    int by_base = H - 26 - btn_stack_h;

    for (int i = SFA_NUM_SPEEDS - 1; i >= 0; i--) {
        int idx = SFA_NUM_SPEEDS - 1 - i;
        int by = by_base + idx * (SFA_VCTRL_BTN_H + SFA_VCTRL_BTN_GAP);

        uint32_t bg = (i == s->speed_level) ? 0xC0332200 : SFA_HUD_BG;
        uint32_t fg = (i == s->speed_level) ? SFA_HUD_ACCENT : SFA_HUD_TEXT;

        sfa_draw_rect(px, W, H, bx, by, bx + SFA_VCTRL_BTN_W, by + SFA_VCTRL_BTN_H, bg);

        char label[8];
        if (i == 0) snprintf(label, sizeof(label), "STOP");
        else if (i == SFA_NUM_SPEEDS - 1) snprintf(label, sizeof(label), "FULL");
        else snprintf(label, sizeof(label), "%d/4", i);
        sr_draw_text_shadow(px, W, H, bx + 4, by + 5, label, fg, SFA_HUD_SHADOW);
    }

    /* Speed name + bar below buttons */
    {
        int sx = bx - 40;  /* shifted left for longer text */
        int sbar_w = SFA_VCTRL_BTN_W + 40;
        int sy = by_base + btn_stack_h + 2;
        sr_draw_text_shadow(px, W, H, sx, sy, sfa_speed_names[s->speed_level],
                             SFA_HUD_ACCENT, SFA_HUD_SHADOW);
        int sbar_h = 4;
        int sby = sy + 9;
        float max_spd = sfa_speed_values[SFA_NUM_SPEEDS - 1];
        sfa_draw_rect(px, W, H, sx, sby, sx + sbar_w, sby + sbar_h, SFA_HUD_BG);
        float target_spd = sfa_speed_values[s->speed_level];
        int tw = (max_spd > 0) ? (int)(sbar_w * target_spd / max_spd) : 0;
        if (tw > 0)
            sfa_draw_rect(px, W, H, sx, sby, sx + tw, sby + sbar_h, 0x60335577);
        int fw = (max_spd > 0) ? (int)(sbar_w * s->current_speed / max_spd) : 0;
        if (fw > 0) {
            uint32_t fc = s->speed_level == SFA_SPEED_FULL ? SFA_HUD_WARN : SFA_HUD_ACCENT;
            sfa_draw_rect(px, W, H, sx, sby, sx + fw, sby + sbar_h, fc);
        }
    }

    /* ── Turn buttons (top-left / top-right) ── */
    {
        int lx = SFA_TURN_BTN_MARGIN;
        int rx = W - SFA_TURN_BTN_W - SFA_TURN_BTN_MARGIN;
        int ty = SFA_TURN_BTN_Y;

        uint32_t l_bg = sfa.touch_turn_left  ? 0xC0332200 : SFA_HUD_BG;
        uint32_t r_bg = sfa.touch_turn_right ? 0xC0332200 : SFA_HUD_BG;
        uint32_t l_fg = sfa.touch_turn_left  ? SFA_HUD_ACCENT : SFA_HUD_TEXT;
        uint32_t r_fg = sfa.touch_turn_right ? SFA_HUD_ACCENT : SFA_HUD_TEXT;

        sfa_draw_rect(px, W, H, lx, ty, lx + SFA_TURN_BTN_W, ty + SFA_TURN_BTN_H, l_bg);
        sfa_draw_rect(px, W, H, rx, ty, rx + SFA_TURN_BTN_W, ty + SFA_TURN_BTN_H, r_bg);

        /* Left arrow: "<" */
        sr_draw_text_shadow(px, W, H, lx + 14, ty + 11, "<", l_fg, SFA_HUD_SHADOW);
        /* Right arrow: ">" */
        sr_draw_text_shadow(px, W, H, rx + 14, ty + 11, ">", r_fg, SFA_HUD_SHADOW);
    }

}

/* ── HUD: Weapon bars (bottom-center, combined button + cooldown) ── */

/* Fixed weapon bar dimensions — must match between draw and click handling */
#define SFA_WBAR_W   76
#define SFA_WBAR_H   20
#define SFA_WBAR_GAP 4
#define SFA_WBAR_PAD 4

static void sfa_draw_weapon_bars(uint32_t *px, int W, int H, sfa_ship *s) {
    char ph_label[20], tp_label[20];

    float ph_pct = 1.0f - (s->phaser_cooldown / SFA_PHASER_COOLDOWN);
    if (ph_pct > 1.0f) ph_pct = 1.0f;
    if (ph_pct < 0.0f) ph_pct = 0.0f;
    bool ph_rdy = ph_pct >= 1.0f;

    if (ph_rdy)
        snprintf(ph_label, sizeof(ph_label), "PHSR [SPC]");
    else
        snprintf(ph_label, sizeof(ph_label), "PHSR %.1f", s->phaser_cooldown);

    float tp_pct = 1.0f - (s->torpedo_cooldown / SFA_TORP_COOLDOWN);
    if (tp_pct > 1.0f) tp_pct = 1.0f;
    if (tp_pct < 0.0f) tp_pct = 0.0f;
    bool tp_rdy = tp_pct >= 1.0f && s->torpedoes_remaining > 0;

    if (s->torpedoes_remaining <= 0)
        snprintf(tp_label, sizeof(tp_label), "TORP EMPTY");
    else if (tp_rdy)
        snprintf(tp_label, sizeof(tp_label), "TORP [F] x%d", s->torpedoes_remaining);
    else
        snprintf(tp_label, sizeof(tp_label), "TORP x%d %.1f", s->torpedoes_remaining, s->torpedo_cooldown);

    int total_w = SFA_WBAR_W * 2 + SFA_WBAR_GAP;
    int x0 = (W - total_w) / 2;
    int y = H - SFA_WBAR_H - 4;

    /* ── Phaser bar ── */
    {
        int x = x0;
        int fill = (int)(SFA_WBAR_W * ph_pct);
        sfa_draw_rect(px, W, H, x, y, x + SFA_WBAR_W, y + SFA_WBAR_H, SFA_HUD_BG);
        uint32_t fill_col = ph_rdy ? 0xFF44FF44 : 0xFF44AACC;
        if (fill > 0) sfa_draw_rect(px, W, H, x, y, x + fill, y + SFA_WBAR_H, fill_col);
        sr_draw_text_shadow(px, W, H, x + SFA_WBAR_PAD, y + 6, ph_label,
                             ph_rdy ? 0xFFFFFFFF : 0xFFCCCCCC, SFA_HUD_SHADOW);
    }

    /* ── Torpedo bar ── */
    {
        int x = x0 + SFA_WBAR_W + SFA_WBAR_GAP;
        int fill = (int)(SFA_WBAR_W * tp_pct);
        sfa_draw_rect(px, W, H, x, y, x + SFA_WBAR_W, y + SFA_WBAR_H, SFA_HUD_BG);
        uint32_t fill_col = tp_rdy ? 0xFF4466FF : 0xFF44AACC;
        if (fill > 0) sfa_draw_rect(px, W, H, x, y, x + fill, y + SFA_WBAR_H, fill_col);
        sr_draw_text_shadow(px, W, H, x + SFA_WBAR_PAD, y + 6, tp_label,
                             tp_rdy ? 0xFFFFFFFF : 0xFFCCCCCC, SFA_HUD_SHADOW);
    }

    /* Target status warning (centered above bars) */
    {
        const char *warn = NULL;
        uint32_t warn_col = 0xFF4466FF;
        if (sfa.selected_npc < 0) {
            warn = "NO TARGET [TAB]";
            warn_col = 0xFF6666AA;
        } else {
            sfa_ship *tgt = &sfa.npcs[sfa.selected_npc];
            if (!tgt->alive) {
                warn = "DESTROYED";
                warn_col = 0xFF4444CC;
            } else {
                float dx = tgt->x - s->x;
                float dz = tgt->z - s->z;
                float dist = sqrtf(dx * dx + dz * dz);
                if (dist > SFA_PHASER_RANGE) {
                    warn = "OUT OF RANGE";
                } else if (!sfa_in_weapon_arc(s, tgt->x, tgt->z, SFA_PHASER_ARC)) {
                    warn = "OUT OF ARC";
                    warn_col = 0xFF44AAFF;
                }
            }
        }
        if (warn) {
            int tw = (int)strlen(warn) * 6;
            sr_draw_text_shadow(px, W, H, (W - tw) / 2, y - 12, warn,
                                 warn_col, SFA_HUD_SHADOW);
        }
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

    /* Player health bar (top-left) — hull only (shields shown in hex) */
    {
        int pcls = s->ship_class;
        if (pcls < 0 || pcls >= SHIP_CLASS_COUNT) pcls = SHIP_CLASS_CRUISER;
        const ship_class_stats *psc = &ship_classes[pcls];
        float hp_pct = s->hull / (float)psc->hull_max;
        if (hp_pct < 0) hp_pct = 0;
        if (hp_pct > 1) hp_pct = 1;

        int bar_w = 80, bar_h = 7;
        int bx = 3, by = 3;

        sfa_draw_rect(px, W, H, bx, by, bx + bar_w, by + bar_h, 0xC0000000);
        int fill = (int)(bar_w * hp_pct);
        uint32_t hp_col = hp_pct > 0.6f ? 0xFF44CC44
                        : hp_pct > 0.3f ? 0xFF44CCCC
                        :                  0xFF4444CC;
        if (fill > 0)
            sfa_draw_rect(px, W, H, bx, by, bx + fill, by + bar_h, hp_col);

        /* Hull percentage label inside bar */
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)(hp_pct * 100));
        sr_draw_text_shadow(px, W, H, bx + 2, by - 1, buf,
                             SFA_HUD_BRIGHT, SFA_HUD_SHADOW);
    }

    /* Shield hex display (bottom-left) */
    sfa_draw_shield_hud(px, W, H, s, 45, H - 100);

    /* Mobile virtual controls */
    sfa_draw_mobile_controls(px, W, H, s);

    /* Weapon bars (bottom-center) */
    sfa_draw_weapon_bars(px, W, H, s);

    /* Top-right buttons: [SRS/LRS] [CLR TGT] [MENU] */
    {
        int mby = 3, mbh = 11;

        /* MENU button (rightmost) */
        int menu_x = W - 32;
        sfa_draw_rect(px, W, H, menu_x, mby, menu_x + 30, mby + mbh, SFA_HUD_BG);
        sr_draw_text_shadow(px, W, H, menu_x + 3, mby + 2, "MENU",
                             0xFF999999, SFA_HUD_SHADOW);

        /* CLR TGT button */
        int clr_x = menu_x - 50;
        uint32_t clr_col = (sfa.selected_npc >= 0) ? SFA_HUD_ACCENT : 0xFF555555;
        sfa_draw_rect(px, W, H, clr_x, mby, clr_x + 48, mby + mbh, SFA_HUD_BG);
        sr_draw_text_shadow(px, W, H, clr_x + 3, mby + 2, "CLR TGT",
                             clr_col, SFA_HUD_SHADOW);

        /* SRS/LRS button */
        int sns_x = clr_x - 32;
        const char *sns_label = sfa.long_range_sensors ? "LRS" : "SRS";
        uint32_t sns_col = sfa.long_range_sensors ? 0xFF44CC44 : SFA_HUD_ACCENT;
        sfa_draw_rect(px, W, H, sns_x, mby, sns_x + 30, mby + mbh, SFA_HUD_BG);
        sr_draw_text_shadow(px, W, H, sns_x + 3, mby + 2, sns_label,
                             sns_col, SFA_HUD_SHADOW);
    }
}

#endif /* SFA_HUD_H */
