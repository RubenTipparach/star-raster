/*  sr_scene_ship_viewer.h — Ship model viewer scene.
 *  Header-only. Depends on sr_app.h, space_fleet/sfa_render.h. */
#ifndef SR_SCENE_SHIP_VIEWER_H
#define SR_SCENE_SHIP_VIEWER_H

/* Ship viewer state */
static struct {
    int  faction;       /* 0=Federation, 1=Klingon */
    int  ship_class;    /* SHIP_CLASS_* */
    float orbit_angle;  /* camera orbit angle */
    float orbit_speed;  /* auto-rotate speed */
    bool  auto_rotate;
    bool  initialized;
} sv;

static void sv_init(void) {
    sv.faction = 0;
    sv.ship_class = SHIP_CLASS_CRUISER;
    sv.orbit_angle = 0.4f;
    sv.orbit_speed = 0.5f;
    sv.auto_rotate = true;
    sv.initialized = true;
}

static void draw_ship_viewer_scene(sr_framebuffer *fb_ptr, float dt) {
    if (!sv.initialized) sv_init();

    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    uint32_t shadow = 0xFF000000;

    /* Clear to dark blue */
    for (int i = 0; i < W * H; i++)
        px[i] = 0xFF1A0A0A;

    /* Auto-rotate */
    if (sv.auto_rotate)
        sv.orbit_angle += sv.orbit_speed * dt;

    /* Camera orbiting origin */
    float cam_dist = 4.0f;
    float cam_h = 1.8f;
    sr_vec3 eye = {
        sinf(sv.orbit_angle) * cam_dist,
        cam_h,
        cosf(sv.orbit_angle) * cam_dist
    };
    sr_vec3 target = { 0, 0.1f, 0 };
    sr_vec3 up = { 0, 1, 0 };

    sr_mat4 view = sr_mat4_lookat(eye, target, up);
    sr_mat4 proj = sr_mat4_perspective(
        45.0f * (3.14159265f / 180.0f),
        (float)W / (float)H,
        0.1f, 50.0f
    );
    sr_mat4 vp = sr_mat4_mul(proj, view);

    /* Draw a ground grid */
    {
        uint32_t grid_col = 0xFF333333;
        for (int g = -3; g <= 3; g++) {
            float gf = (float)g;
            /* Z lines */
            int sx0, sy0, sx1, sy1; float sw0, sw1;
            if (sfa_project_to_screen(&vp, gf, 0, -3.0f, W, H, &sx0, &sy0, &sw0) &&
                sfa_project_to_screen(&vp, gf, 0,  3.0f, W, H, &sx1, &sy1, &sw1)) {
                int dx = sx1-sx0, dy = sy1-sy0;
                int steps = (dx<0?-dx:dx) > (dy<0?-dy:dy) ? (dx<0?-dx:dx) : (dy<0?-dy:dy);
                if (steps < 1) steps = 1;
                for (int j = 0; j <= steps; j++) {
                    int lx = sx0 + dx*j/steps, ly = sy0 + dy*j/steps;
                    if (lx >= 0 && lx < W && ly >= 0 && ly < H && ((lx+ly)&1)==0)
                        px[ly*W+lx] = grid_col;
                }
            }
            /* X lines */
            if (sfa_project_to_screen(&vp, -3.0f, 0, gf, W, H, &sx0, &sy0, &sw0) &&
                sfa_project_to_screen(&vp,  3.0f, 0, gf, W, H, &sx1, &sy1, &sw1)) {
                int dx = sx1-sx0, dy = sy1-sy0;
                int steps = (dx<0?-dx:dx) > (dy<0?-dy:dy) ? (dx<0?-dx:dx) : (dy<0?-dy:dy);
                if (steps < 1) steps = 1;
                for (int j = 0; j <= steps; j++) {
                    int lx = sx0 + dx*j/steps, ly = sy0 + dy*j/steps;
                    if (lx >= 0 && lx < W && ly >= 0 && ly < H && ((lx+ly)&1)==0)
                        px[ly*W+lx] = grid_col;
                }
            }
        }
    }

    /* Draw the selected ship at origin */
    {
        sfa_ship dummy = {0};
        dummy.x = 0; dummy.z = 0;
        dummy.heading = 0; dummy.visual_heading = 0;
        dummy.ship_class = sv.ship_class;
        dummy.current_speed = 0;

        if (sv.faction == 0) {
            sfa_draw_ship(fb_ptr, &vp, &dummy);
        } else {
            sfa_draw_target_ship(fb_ptr, &vp, 0, 0, 0, sv.ship_class);
        }
    }

    /* HUD */
    const char *faction_name = sv.faction == 0 ? "FEDERATION" : "KLINGON";
    const char *class_name = ship_class_names[sv.ship_class];
    const ship_class_stats *sc = &ship_classes[sv.ship_class];

    char buf[64];

    /* Title */
    snprintf(buf, sizeof(buf), "%s %s", faction_name, class_name);
    sr_draw_text_shadow(px, W, H, W/2 - 50, 5, buf, 0xFFFFFFFF, shadow);

    /* Stats panel */
    int sy = 20;
    snprintf(buf, sizeof(buf), "Hull:    %d", sc->hull_max);
    sr_draw_text_shadow(px, W, H, 5, sy, buf, 0xFF88CCFF, shadow); sy += 10;
    snprintf(buf, sizeof(buf), "Shields: %.0f x6", sc->shield_max);
    sr_draw_text_shadow(px, W, H, 5, sy, buf, 0xFF88CCFF, shadow); sy += 10;
    snprintf(buf, sizeof(buf), "Speed:   %.1fx", sc->speed_mult);
    sr_draw_text_shadow(px, W, H, 5, sy, buf, 0xFF88CCFF, shadow); sy += 10;
    snprintf(buf, sizeof(buf), "Turn:    %.1fx", sc->turn_mult);
    sr_draw_text_shadow(px, W, H, 5, sy, buf, 0xFF88CCFF, shadow); sy += 10;
    if (sc->cost > 0) {
        snprintf(buf, sizeof(buf), "Cost:    %d CR", sc->cost);
        sr_draw_text_shadow(px, W, H, 5, sy, buf, 0xFF44DD44, shadow);
    } else {
        sr_draw_text_shadow(px, W, H, 5, sy, "Cost:    FREE", 0xFF44DD44, shadow);
    }

    /* Controls hint */
    sr_draw_text_shadow(px, W, H, 5, H - 22,
                         "LEFT/RIGHT: CLASS  UP/DOWN: FACTION", 0xFF777777, shadow);
    sr_draw_text_shadow(px, W, H, 5, H - 12,
                         "SPACE: ROTATE  ESC: BACK", 0xFF777777, shadow);

    /* Touch buttons */
    {
        int bw = 30, bh = 14;

        /* < > class buttons */
        nm_draw_rect(px, W, H, W - 68, H - 30, W - 68 + bw, H - 30 + bh, 0xC0000000);
        sr_draw_text_shadow(px, W, H, W - 62, H - 27, "<", 0xFFCCCCCC, shadow);
        nm_draw_rect(px, W, H, W - 34, H - 30, W - 34 + bw, H - 30 + bh, 0xC0000000);
        sr_draw_text_shadow(px, W, H, W - 28, H - 27, ">", 0xFFCCCCCC, shadow);

        /* Faction toggle */
        nm_draw_rect(px, W, H, W - 68, H - 48, W - 4, H - 48 + bh, 0xC0000000);
        sr_draw_text_shadow(px, W, H, W - 62, H - 45,
                             sv.faction == 0 ? "FED" : "KLG", 0xFFCCCCCC, shadow);
    }

    /* MENU button */
    {
        int mbx = W - 32, mby = 3, mbw = 30, mbh = 11;
        for (int ry = mby; ry < mby + mbh && ry < H; ry++)
            for (int rx = mbx; rx < mbx + mbw && rx < W; rx++)
                if (ry >= 0 && rx >= 0) px[ry * W + rx] = 0xC0000000;
        sr_draw_text_shadow(px, W, H, mbx + 3, mby + 2, "MENU", 0xFF999999, shadow);
    }
}

static void sv_handle_key(int key_code) {
    /* Using raw ints to avoid sokol dependency */
    enum { K_LEFT = 263, K_RIGHT = 262, K_UP = 265, K_DOWN = 264, K_SPACE = 32, K_ESC = 256 };
    switch (key_code) {
        case K_LEFT:
            sv.ship_class--;
            if (sv.ship_class < 0) sv.ship_class = SHIP_CLASS_COUNT - 1;
            break;
        case K_RIGHT:
            sv.ship_class++;
            if (sv.ship_class >= SHIP_CLASS_COUNT) sv.ship_class = 0;
            break;
        case K_UP:
        case K_DOWN:
            sv.faction = sv.faction ? 0 : 1;
            break;
        case K_SPACE:
            sv.auto_rotate = !sv.auto_rotate;
            break;
        case K_ESC:
            app_state = STATE_MENU;
            break;
        default: break;
    }
}

static void sv_handle_click(float fx, float fy) {
    int W = FB_WIDTH, H = FB_HEIGHT;

    /* MENU button */
    if (fx >= W - 32 && fx <= W - 2 && fy >= 3 && fy <= 14) {
        app_state = STATE_MENU;
        return;
    }

    /* < button */
    if (fx >= W - 68 && fx <= W - 38 && fy >= H - 30 && fy <= H - 16) {
        sv.ship_class--;
        if (sv.ship_class < 0) sv.ship_class = SHIP_CLASS_COUNT - 1;
        return;
    }
    /* > button */
    if (fx >= W - 34 && fx <= W - 4 && fy >= H - 30 && fy <= H - 16) {
        sv.ship_class++;
        if (sv.ship_class >= SHIP_CLASS_COUNT) sv.ship_class = 0;
        return;
    }
    /* Faction toggle */
    if (fx >= W - 68 && fx <= W - 4 && fy >= H - 48 && fy <= H - 34) {
        sv.faction = sv.faction ? 0 : 1;
        return;
    }
}

#endif /* SR_SCENE_SHIP_VIEWER_H */
