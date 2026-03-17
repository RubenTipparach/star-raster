/*  sr_menu.h — Stats overlay and menu.
 *  Single-TU header-only. Depends on sr_app.h, sr_font.h. */
#ifndef SR_MENU_H
#define SR_MENU_H

static void draw_stats(sr_framebuffer *fb_ptr, int tris) {
    char buf[64];
    uint32_t white  = 0xFFFFFFFF;
    uint32_t shadow = 0xFF000000;

    if (current_scene != SCENE_DUNGEON && current_scene != SCENE_SPACE_FLEET && current_scene != SCENE_NODE_MAP && current_scene != SCENE_SHIP_VIEWER) {
        snprintf(buf, sizeof(buf), "FPS: %d", fps_display);
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            3, 3, buf, white, shadow);

        snprintf(buf, sizeof(buf), "TRIS: %d", tris);
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            3, 13, buf, white, shadow);

        snprintf(buf, sizeof(buf), "%dX%d  %s", FB_WIDTH, FB_HEIGHT,
                 scene_names[current_scene]);
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            3, 23, buf, white, shadow);

        /* MENU button (top-right corner) */
        {
            int mbx = FB_WIDTH - 32, mby = 3, mbw = 30, mbh = 11;
            uint32_t *px = fb_ptr->color;
            for (int ry = mby; ry < mby + mbh && ry < FB_HEIGHT; ry++)
                for (int rx = mbx; rx < mbx + mbw && rx < FB_WIDTH; rx++)
                    if (ry >= 0 && rx >= 0) px[ry * FB_WIDTH + rx] = 0xC0000000;
            sr_draw_text_shadow(px, fb_ptr->width, fb_ptr->height,
                                mbx + 3, mby + 2, "MENU", 0xFF999999, shadow);
        }
    }

    /* Recording indicator */
    if (sr_gif_is_recording()) {
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            FB_WIDTH - 30, 3, "REC", 0xFF0000FF, shadow);
    }

    /* Night mode hint (neighborhood only) */
    if (current_scene == SCENE_NEIGHBORHOOD) {
        const char *ntxt = night_mode ? "N = DAY" : "N = NIGHT";
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            3, FB_HEIGHT - 12, ntxt, 0xFF999999, shadow);
    }

    /* Palette scene lighting controls */
    if (current_scene == SCENE_PALETTE_HOUSE) {
        uint32_t sel_col  = 0xFF55CCFF;
        uint32_t dim_col  = 0xFF999999;
        uint32_t btn_bg   = 0xC0000000;
        uint32_t btn_hi   = 0xC0332200;
        int W = fb_ptr->width, H = fb_ptr->height;
        uint32_t *px = fb_ptr->color;

        #define DRAW_RECT(x0,y0,x1,y1,col) do { \
            for (int ry = (y0); ry < (y1) && ry < H; ry++) \
                for (int rx = (x0); rx < (x1) && rx < W; rx++) \
                    if (ry >= 0 && rx >= 0) px[ry * W + rx] = (col); \
        } while(0)

        int bx = 2, bw = 100;
        int by0 = H - 62;
        int by1 = H - 50;
        int by2 = H - 38;
        int by3 = H - 26;
        int bh = 11;

        DRAW_RECT(bx, by0, bx + bw, by0 + bh, adjusting_ambient ? btn_hi : btn_bg);
        snprintf(buf, sizeof(buf), "AMB: %.2f", pal_ambient);
        sr_draw_text_shadow(px, W, H, bx + 2, by0 + 2, buf,
                            adjusting_ambient ? sel_col : dim_col, shadow);
        DRAW_RECT(bx + bw + 2, by0, bx + bw + 14, by0 + bh, btn_bg);
        sr_draw_text_shadow(px, W, H, bx + bw + 4, by0 + 2, "-", white, shadow);
        DRAW_RECT(bx + bw + 16, by0, bx + bw + 28, by0 + bh, btn_bg);
        sr_draw_text_shadow(px, W, H, bx + bw + 18, by0 + 2, "+", white, shadow);

        DRAW_RECT(bx, by1, bx + bw, by1 + bh, !adjusting_ambient ? btn_hi : btn_bg);
        snprintf(buf, sizeof(buf), "LIGHT: %.1fx", pal_light_mult);
        sr_draw_text_shadow(px, W, H, bx + 2, by1 + 2, buf,
                            !adjusting_ambient ? sel_col : dim_col, shadow);
        DRAW_RECT(bx + bw + 2, by1, bx + bw + 14, by1 + bh, btn_bg);
        sr_draw_text_shadow(px, W, H, bx + bw + 4, by1 + 2, "-", white, shadow);
        DRAW_RECT(bx + bw + 16, by1, bx + bw + 28, by1 + bh, btn_bg);
        sr_draw_text_shadow(px, W, H, bx + bw + 18, by1 + 2, "+", white, shadow);

        int sw = pixel_lighting ? 84 : 90;
        DRAW_RECT(bx, by2, bx + sw, by2 + bh, pixel_lighting ? btn_hi : btn_bg);
        snprintf(buf, sizeof(buf), "SHADING: %s", pixel_lighting ? "PIXEL" : "VERTEX");
        sr_draw_text_shadow(px, W, H, bx + 2, by2 + 2, buf,
                            pixel_lighting ? sel_col : dim_col, shadow);

        int shw = shadows_enabled ? 72 : 78;
        DRAW_RECT(bx, by3, bx + shw, by3 + bh, shadows_enabled ? btn_hi : btn_bg);
        snprintf(buf, sizeof(buf), "SHADOWS: %s", shadows_enabled ? "ON" : "OFF");
        sr_draw_text_shadow(px, W, H, bx + 2, by3 + 2, buf,
                            shadows_enabled ? sel_col : dim_col, shadow);

        #undef DRAW_RECT
    }
}

static void draw_menu(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    (void)H;

    /* Darken framebuffer */
    for (int i = 0; i < W * H; i++) {
        uint32_t c = px[i];
        uint8_t r = ((c      ) & 0xFF) >> 2;
        uint8_t g = ((c >>  8) & 0xFF) >> 2;
        uint8_t b = ((c >> 16) & 0xFF) >> 2;
        uint8_t a = (c >> 24) & 0xFF;
        px[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }

    uint32_t white  = 0xFFFFFFFF;
    uint32_t gray   = 0xFF999999;
    uint32_t yellow = 0xFF00FFFF;
    uint32_t shadow = 0xFF000000;

    sr_draw_text_shadow(px, W, H, 180, 50, "STARRASTER", white, shadow);
    sr_draw_text_shadow(px, W, H, 150, 90, "SELECT SCENE:", gray, shadow);

    for (int i = 0; i < SCENE_MENU_COUNT; i++) {
        char line[64];
        snprintf(line, sizeof(line), "[%d]  %s", i + 1, scene_names[i]);
        uint32_t color = (i == menu_cursor) ? yellow : white;
        sr_draw_text_shadow(px, W, H, 150, 115 + i * 15, line, color, shadow);
    }

}

static void draw_sfa_submenu(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    /* Darken framebuffer */
    for (int i = 0; i < W * H; i++) {
        uint32_t c = px[i];
        uint8_t r = ((c      ) & 0xFF) >> 2;
        uint8_t g = ((c >>  8) & 0xFF) >> 2;
        uint8_t b = ((c >> 16) & 0xFF) >> 2;
        uint8_t a = (c >> 24) & 0xFF;
        px[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }

    uint32_t white  = 0xFFFFFFFF;
    uint32_t gray   = 0xFF999999;
    uint32_t yellow = 0xFF00FFFF;
    uint32_t shadow = 0xFF000000;

    sr_draw_text_shadow(px, W, H, 160, 70, "SPACE FLEET", white, shadow);
    sr_draw_text_shadow(px, W, H, 150, 100, "SELECT MODE:", gray, shadow);

    const char *opts[] = { "INSTANT ACTION", "CAMPAIGN", "SHIP VIEWER" };
    for (int i = 0; i < 3; i++) {
        char line[64];
        snprintf(line, sizeof(line), "[%d]  %s", i + 1, opts[i]);
        uint32_t color = (i == sfa_submenu_cursor) ? yellow : white;
        sr_draw_text_shadow(px, W, H, 150, 125 + i * 15, line, color, shadow);
    }

    sr_draw_text_shadow(px, W, H, 150, 170, "ESC = BACK", gray, shadow);
}

#endif /* SR_MENU_H */
