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
    if (campaign.campaign_active) {
        sr_draw_text_shadow(px, W, H, lx, ly, ship_class_names[campaign.player_ship_class], white, shadow);
    } else {
        sr_draw_text_shadow(px, W, H, lx, ly, "Federation Cruiser", white, shadow);
    }
    ly += 16;

    {
        int pcls = campaign.campaign_active ? campaign.player_ship_class : SHIP_CLASS_CRUISER;
        const ship_class_stats *ps = &ship_classes[pcls];
        char sbuf[48];
        snprintf(sbuf, sizeof(sbuf), "Hull:      %d HP", ps->hull_max);
        sr_draw_text_shadow(px, W, H, lx, ly, sbuf, gray, shadow); ly += 10;
        snprintf(sbuf, sizeof(sbuf), "Shields:   6 x %.0f", ps->shield_max);
        sr_draw_text_shadow(px, W, H, lx, ly, sbuf, gray, shadow); ly += 10;
        snprintf(sbuf, sizeof(sbuf), "Max speed: %.0f m/s", 6.0f * ps->speed_mult);
        sr_draw_text_shadow(px, W, H, lx, ly, sbuf, gray, shadow); ly += 10;
        snprintf(sbuf, sizeof(sbuf), "Turn rate: %.0f deg/s", 115.0f * ps->turn_mult);
        sr_draw_text_shadow(px, W, H, lx, ly, sbuf, gray, shadow); ly += 16;
    }

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
        char buf[48];
        /* List enemy ship classes */
        for (int ei = 0; ei < sfa.npc_count && ei < SFA_MAX_NPC; ei++) {
            int ecls = sfa.npcs[ei].ship_class;
            if (ecls < 0 || ecls >= SHIP_CLASS_COUNT) ecls = SHIP_CLASS_DESTROYER;
            if (sfa.npcs[ei].is_boss) {
                snprintf(buf, sizeof(buf), "Klingon %s [BOSS]", ship_class_names[ecls]);
                sr_draw_text_shadow(px, W, H, rx, ry, buf, warn, shadow);
            } else {
                snprintf(buf, sizeof(buf), "Klingon %s", ship_class_names[ecls]);
                sr_draw_text_shadow(px, W, H, rx, ry, buf, white, shadow);
            }
            ry += 10;
        }
    }
    ry += 6;
    {
        /* Show stats of first enemy class as representative */
        int ecls = sfa.npcs[0].ship_class;
        if (ecls < 0 || ecls >= SHIP_CLASS_COUNT) ecls = SHIP_CLASS_DESTROYER;
        const ship_class_stats *es = &ship_classes[ecls];
        char buf[48];
        snprintf(buf, sizeof(buf), "Hull:      %d HP", es->hull_max);
        sr_draw_text_shadow(px, W, H, rx, ry, buf, gray, shadow); ry += 10;
        snprintf(buf, sizeof(buf), "Shields:   6 x %.0f", es->shield_max);
        sr_draw_text_shadow(px, W, H, rx, ry, buf, gray, shadow); ry += 10;
        snprintf(buf, sizeof(buf), "Max speed: %.0f m/s", 6.0f * es->speed_mult);
        sr_draw_text_shadow(px, W, H, rx, ry, buf, gray, shadow); ry += 10;
        snprintf(buf, sizeof(buf), "Turn rate: %.0f deg/s", 115.0f * es->turn_mult);
        sr_draw_text_shadow(px, W, H, rx, ry, buf, gray, shadow); ry += 16;
    }

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
    {
        int pcls = sfa.player.ship_class;
        if (pcls < 0 || pcls >= SHIP_CLASS_COUNT) pcls = SHIP_CLASS_CRUISER;
        float hull_max = (float)ship_classes[pcls].hull_max;
        int hull_pct = (hull_max > 0) ? (int)(sfa.player.hull / hull_max * 100.0f) : 0;
        snprintf(buf, sizeof(buf), "Hull integrity:    %d%%", hull_pct);
        uint32_t hull_col = (sfa.player.hull > hull_max * 0.5f) ? green : 0xFF4466FF;
        sr_draw_text_shadow(px, W, H, cx, y, buf, hull_col, shadow); y += 12;
    }

    {
        int scls = sfa.player.ship_class;
        if (scls < 0 || scls >= SHIP_CLASS_COUNT) scls = SHIP_CLASS_CRUISER;
        float shield_total_max = 6.0f * ship_classes[scls].shield_max;
        float total_shields = 0;
        for (int i = 0; i < 6; i++) total_shields += sfa.player.shields[i];
        float sh_pct = (shield_total_max > 0) ? (total_shields / shield_total_max * 100.0f) : 0;
        snprintf(buf, sizeof(buf), "Shields remaining: %.0f%%", sh_pct);
    }
    sr_draw_text_shadow(px, W, H, cx, y, buf, gray, shadow); y += 12;

    snprintf(buf, sizeof(buf), "Torpedoes left:    %d", sfa.player.torpedoes_remaining);
    sr_draw_text_shadow(px, W, H, cx, y, buf, gray, shadow); y += 16;

    /* Campaign reward info */
    if (campaign.campaign_active) {
        sr_draw_text_shadow(px, W, H, cx, y, "MISSION REWARD", accent, shadow); y += 14;
        snprintf(buf, sizeof(buf), "Credits earned:    %d", campaign.encounter_reward);
        sr_draw_text_shadow(px, W, H, cx, y, buf, green, shadow);
    }

    /* Prompt */
    float blink = sinf(sfa.time * 4.0f);
    if (blink > 0) {
        const char *prompt = campaign.campaign_active ? "Click to return to sector map" : "Click to return home";
        int tw2 = (int)strlen(prompt) * 6;
        sr_draw_text_shadow(px, W, H, (W - tw2)/2, H - 24, prompt, white, shadow);
    }
}

#endif /* SFA_SCREENS_H */
