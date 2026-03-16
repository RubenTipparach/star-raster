/*  sfa_screens.h — Briefing, victory overlay, and stats summary screens. Header-only. Depends on sfa_types.h. */
#ifndef SFA_SCREENS_H
#define SFA_SCREENS_H

/* ── Briefing screen ─────────────────────────────────────────────── */

static void sfa_draw_briefing(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    /* Dark background */
    for (int i = 0; i < W * H; i++) px[i] = SFA_BG_COLOR;

    uint32_t white  = 0xFFFFFFFF;
    uint32_t gray   = 0xFFAAAAAA;
    uint32_t accent = SFA_HUD_ACCENT;
    uint32_t warn   = 0xFF4466FF;
    uint32_t green  = 0xFF44CC44;
    uint32_t shadow = SFA_HUD_SHADOW;

    /* Title */
    sr_draw_text_shadow(px, W, H, (W - 18*6)/2, 16, "SPACE FLEET ASSAULT", white, shadow);
    sr_draw_text_shadow(px, W, H, (W - 15*6)/2, 30, "MISSION BRIEFING", accent, shadow);

    /* Divider */
    sfa_draw_rect(px, W, H, W/2 - 100, 42, W/2 + 100, 43, 0xFF444444);

    /* YOUR SHIP section (left half) */
    int lx = 30;
    int ly = 52;
    sr_draw_text_shadow(px, W, H, lx, ly, "YOUR SHIP", accent, shadow);
    ly += 14;
    sr_draw_text_shadow(px, W, H, lx, ly, "Federation Cruiser", white, shadow);
    ly += 16;

    sr_draw_text_shadow(px, W, H, lx, ly, "Hull:      100 HP", gray, shadow);  ly += 10;
    sr_draw_text_shadow(px, W, H, lx, ly, "Shields:   6 x 100", gray, shadow); ly += 10;
    sr_draw_text_shadow(px, W, H, lx, ly, "Max speed: 12 m/s", gray, shadow);  ly += 10;
    sr_draw_text_shadow(px, W, H, lx, ly, "Turn rate: 115 deg/s", gray, shadow); ly += 16;

    sr_draw_text_shadow(px, W, H, lx, ly, "Weapons", accent, shadow); ly += 12;
    sr_draw_text_shadow(px, W, H, lx, ly,     "Phasers",   white, shadow);
    sr_draw_text_shadow(px, W, H, lx+80, ly,  "150 deg arc", gray, shadow);
    sr_draw_text_shadow(px, W, H, lx+160, ly, "8 dmg", gray, shadow);
    ly += 10;
    sr_draw_text_shadow(px, W, H, lx, ly,     "Torpedoes", white, shadow);
    sr_draw_text_shadow(px, W, H, lx+80, ly,  "30 deg arc", gray, shadow);
    sr_draw_text_shadow(px, W, H, lx+160, ly, "25 dmg", gray, shadow);
    ly += 12;
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Torpedo ammo: %d", sfa.player.torpedoes_remaining);
        sr_draw_text_shadow(px, W, H, lx, ly, buf, gray, shadow);
    }

    /* ENEMY section (right half) */
    int rx = W / 2 + 10;
    int ry = 52;
    sr_draw_text_shadow(px, W, H, rx, ry, "HOSTILES", warn, shadow);
    ry += 14;
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%dx Klingon Bird of Prey", sfa.npc_count);
        sr_draw_text_shadow(px, W, H, rx, ry, buf, white, shadow);
    }
    ry += 16;

    sr_draw_text_shadow(px, W, H, rx, ry, "Hull:      100 HP", gray, shadow);  ry += 10;
    sr_draw_text_shadow(px, W, H, rx, ry, "Shields:   6 x 100", gray, shadow); ry += 10;
    sr_draw_text_shadow(px, W, H, rx, ry, "Max speed: 6 m/s", gray, shadow);   ry += 10;
    sr_draw_text_shadow(px, W, H, rx, ry, "Turn rate: 115 deg/s", gray, shadow); ry += 16;

    sr_draw_text_shadow(px, W, H, rx, ry, "Weapons", warn, shadow); ry += 12;
    sr_draw_text_shadow(px, W, H, rx, ry,     "Disruptors", white, shadow);
    sr_draw_text_shadow(px, W, H, rx+80, ry,  "150 deg arc", gray, shadow);
    sr_draw_text_shadow(px, W, H, rx+160, ry, "5.6 dmg", gray, shadow);
    ry += 10;
    sr_draw_text_shadow(px, W, H, rx, ry,     "Torpedoes", white, shadow);
    sr_draw_text_shadow(px, W, H, rx+80, ry,  "30 deg arc", gray, shadow);
    sr_draw_text_shadow(px, W, H, rx+160, ry, "25 dmg", gray, shadow);

    /* Objective */
    int oy = H - 56;
    sfa_draw_rect(px, W, H, W/2 - 100, oy - 4, W/2 + 100, oy - 3, 0xFF444444);
    sr_draw_text_shadow(px, W, H, (W - 9*6)/2, oy, "OBJECTIVE", accent, shadow);
    sr_draw_text_shadow(px, W, H, (W - 28*6)/2, oy + 12, "Destroy all hostile vessels", green, shadow);

    /* Prompt */
    float blink = sinf(sfa.time * 4.0f);
    if (blink > 0) {
        sr_draw_text_shadow(px, W, H, (W - 22*6)/2, H - 20,
                             "Click to begin mission", white, shadow);
    }
}

/* ── Victory overlay ─────────────────────────────────────────────── */

static void sfa_draw_victory_overlay(uint32_t *px, int W, int H) {
    /* Darken the scene */
    for (int i = 0; i < W * H; i++) {
        uint32_t c = px[i];
        uint8_t r = ((c      ) & 0xFF) >> 1;
        uint8_t g = ((c >>  8) & 0xFF) >> 1;
        uint8_t b = ((c >> 16) & 0xFF) >> 1;
        uint8_t a = (c >> 24) & 0xFF;
        px[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }

    uint32_t white  = 0xFFFFFFFF;
    uint32_t accent = SFA_HUD_ACCENT;
    uint32_t shadow = SFA_HUD_SHADOW;

    sr_draw_text_shadow(px, W, H, (W - 16*6)/2, H/2 - 20,
                         "MISSION COMPLETE", white, shadow);

    /* Countdown */
    char buf[32];
    int secs = (int)ceilf(sfa.phase_timer);
    if (secs < 0) secs = 0;
    snprintf(buf, sizeof(buf), "Debrief in %d...", secs);
    int tw = (int)strlen(buf) * 6;
    sr_draw_text_shadow(px, W, H, (W - tw)/2, H/2 + 4, buf, accent, shadow);
}

/* ── Stats summary screen ────────────────────────────────────────── */

static void sfa_draw_stats_screen(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    /* Dark background */
    for (int i = 0; i < W * H; i++) px[i] = SFA_BG_COLOR;

    uint32_t white  = 0xFFFFFFFF;
    uint32_t gray   = 0xFFAAAAAA;
    uint32_t accent = SFA_HUD_ACCENT;
    uint32_t green  = 0xFF44CC44;
    uint32_t shadow = SFA_HUD_SHADOW;

    sr_draw_text_shadow(px, W, H, (W - 15*6)/2, 24, "MISSION DEBRIEF", white, shadow);
    sfa_draw_rect(px, W, H, W/2 - 80, 38, W/2 + 80, 39, 0xFF444444);

    int cx = W / 2 - 80;
    int y = 50;
    char buf[48];

    sr_draw_text_shadow(px, W, H, cx, y, "COMBAT RESULTS", accent, shadow);
    y += 16;

    snprintf(buf, sizeof(buf), "Enemies destroyed: %d/%d",
             sfa.stats.enemies_destroyed, sfa.npc_count);
    sr_draw_text_shadow(px, W, H, cx, y, buf, white, shadow); y += 12;

    snprintf(buf, sizeof(buf), "Phasers fired:     %d", sfa.stats.phasers_fired);
    sr_draw_text_shadow(px, W, H, cx, y, buf, gray, shadow); y += 12;

    snprintf(buf, sizeof(buf), "Torpedoes fired:   %d", sfa.stats.torpedoes_fired);
    sr_draw_text_shadow(px, W, H, cx, y, buf, gray, shadow); y += 12;

    snprintf(buf, sizeof(buf), "Damage dealt:      %.0f", sfa.stats.damage_dealt);
    sr_draw_text_shadow(px, W, H, cx, y, buf, gray, shadow); y += 12;

    snprintf(buf, sizeof(buf), "Damage taken:      %.0f", sfa.stats.damage_taken);
    sr_draw_text_shadow(px, W, H, cx, y, buf, gray, shadow); y += 16;

    /* Combat time */
    int mins = (int)(sfa.stats.combat_time / 60.0f);
    int secs = (int)(sfa.stats.combat_time) % 60;
    snprintf(buf, sizeof(buf), "Mission time:      %d:%02d", mins, secs);
    sr_draw_text_shadow(px, W, H, cx, y, buf, gray, shadow); y += 16;

    /* Player ship status */
    sr_draw_text_shadow(px, W, H, cx, y, "SHIP STATUS", accent, shadow); y += 14;
    snprintf(buf, sizeof(buf), "Hull integrity:    %d%%", (int)sfa.player.hull);
    uint32_t hull_col = sfa.player.hull > 50 ? green : 0xFF4466FF;
    sr_draw_text_shadow(px, W, H, cx, y, buf, hull_col, shadow); y += 12;

    float total_shields = 0;
    for (int i = 0; i < 6; i++) total_shields += sfa.player.shields[i];
    snprintf(buf, sizeof(buf), "Shields remaining: %.0f%%", total_shields / 6.0f);
    sr_draw_text_shadow(px, W, H, cx, y, buf, gray, shadow); y += 12;

    snprintf(buf, sizeof(buf), "Torpedoes left:    %d", sfa.player.torpedoes_remaining);
    sr_draw_text_shadow(px, W, H, cx, y, buf, gray, shadow);

    /* Prompt */
    float blink = sinf(sfa.time * 4.0f);
    if (blink > 0) {
        sr_draw_text_shadow(px, W, H, (W - 20*6)/2, H - 24,
                             "Click to return home", white, shadow);
    }
}

#endif /* SFA_SCREENS_H */
