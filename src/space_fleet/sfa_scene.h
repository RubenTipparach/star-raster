/*  sfa_scene.h — Main scene draw entry point. Header-only. Depends on all other sfa_*.h files. */
#ifndef SFA_SCENE_H
#define SFA_SCENE_H

/* ── Main scene draw ─────────────────────────────────────────────── */

static void draw_space_fleet_scene(sr_framebuffer *fb_ptr, float dt) {
    if (!sfa.initialized) sfa_init();

    sfa.time += dt;  /* always tick time for blinking effects */

    /* Handle non-combat phases */
    if (sfa.phase == SFA_PHASE_BRIEFING) {
        sfa_draw_briefing(fb_ptr);
        return;
    }
    if (sfa.phase == SFA_PHASE_STATS) {
        sfa_draw_stats_screen(fb_ptr);
        return;
    }

    sfa_update(dt);

    sfa_ship *s = &sfa.player;

    /* ── Camera: use cam_target_yaw which blends toward selected target ── */
    float cam_yaw = sfa.cam_target_yaw;
    float cam_back_x =  sinf(cam_yaw) * SFA_CAM_BACK;
    float cam_back_z = -cosf(cam_yaw) * SFA_CAM_BACK;
    float look_fwd_x = -sinf(cam_yaw) * SFA_CAM_LOOK_AHEAD;
    float look_fwd_z =  cosf(cam_yaw) * SFA_CAM_LOOK_AHEAD;

    sr_vec3 eye = {
        s->x + cam_back_x,
        SFA_CAM_HEIGHT,
        s->z + cam_back_z
    };
    sr_vec3 cam_target = {
        s->x + look_fwd_x,
        0.0f,
        s->z + look_fwd_z
    };
    sr_vec3 up = { 0, 1, 0 };

    sr_mat4 view = sr_mat4_lookat(eye, cam_target, up);
    sr_mat4 proj = sr_mat4_perspective(
        45.0f * SFA_DEG2RAD,
        (float)FB_WIDTH / (float)FB_HEIGHT,
        0.5f, 200.0f
    );
    sr_mat4 vp = sr_mat4_mul(proj, view);

    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    /* Cache VP matrix + inverse for input-time hover and plane casting */
    sfa.last_vp = vp;
    sfa_mat4_invert(&vp, &sfa.last_inv_vp);
    sfa.last_eye = eye;
    sfa.last_fb_w = W;
    sfa.last_fb_h = H;

    /* Update hover detection */
    sfa_update_hover(&vp, W, H);

    /* Draw world */
    sfa_draw_starfield(fb_ptr, &vp, s->x, s->z);
    sfa_draw_arena_boundary(fb_ptr, &vp, s->x, s->z);

    /* Draw player ship */
    sfa_draw_ship(fb_ptr, &vp, s);

    /* ── 3D Steering disc around player ship ── */
    {
        float disc_r = SFA_STEER_DISC_R;
        float disc_y = 0.05f;     /* just above ground */
        int segments = 32;
        uint32_t ring_col = 0x80888888; /* dim gray ring */

        /* Draw disc ring as 2D projected lines */
        for (int i = 0; i < segments; i++) {
            float a0 = (float)i / segments * SFA_TWO_PI;
            float a1 = (float)(i + 1) / segments * SFA_TWO_PI;

            float wx0 = s->x + cosf(a0) * disc_r;
            float wz0 = s->z + sinf(a0) * disc_r;
            float wx1 = s->x + cosf(a1) * disc_r;
            float wz1 = s->z + sinf(a1) * disc_r;

            /* Dithered: skip every other segment */
            if (i & 1) continue;

            int sx0, sy0, sx1, sy1;
            float sw0, sw1;
            if (!sfa_project_to_screen(&vp, wx0, disc_y, wz0, W, H, &sx0, &sy0, &sw0)) continue;
            if (!sfa_project_to_screen(&vp, wx1, disc_y, wz1, W, H, &sx1, &sy1, &sw1)) continue;

            int ldx = sx1 - sx0, ldy = sy1 - sy0;
            int steps = (ldx < 0 ? -ldx : ldx) > (ldy < 0 ? -ldy : ldy)
                      ? (ldx < 0 ? -ldx : ldx) : (ldy < 0 ? -ldy : ldy);
            if (steps < 1) steps = 1;
            for (int j = 0; j <= steps; j++) {
                int lx = sx0 + ldx * j / steps;
                int ly = sy0 + ldy * j / steps;
                if (lx >= 0 && lx < W && ly >= 0 && ly < H)
                    px[ly * W + lx] = ring_col;
            }
        }

        /* Target heading indicator on disc — 2D projected line */
        {
            float angle_diff = sfa_normalize_angle(s->target_heading - s->heading);
            if (angle_diff > 0.01f || angle_diff < -0.01f) {
                float th = s->target_heading;
                float tx = s->x - sinf(th) * disc_r;
                float tz = s->z + cosf(th) * disc_r;
                uint32_t tgt_col = 0xFF00AAFF; /* orange */

                int sx0, sy0, sx1, sy1;
                float sw0, sw1;
                if (sfa_project_to_screen(&vp, s->x, 0.3f, s->z, W, H, &sx0, &sy0, &sw0) &&
                    sfa_project_to_screen(&vp, tx, 0.3f, tz, W, H, &sx1, &sy1, &sw1)) {
                    int ldx = sx1 - sx0, ldy = sy1 - sy0;
                    int steps = (ldx < 0 ? -ldx : ldx) > (ldy < 0 ? -ldy : ldy)
                              ? (ldx < 0 ? -ldx : ldx) : (ldy < 0 ? -ldy : ldy);
                    if (steps < 1) steps = 1;
                    for (int j = 0; j <= steps; j++) {
                        int lx = sx0 + ldx * j / steps;
                        int ly = sy0 + ldy * j / steps;
                        if (lx >= 0 && lx < W && ly >= 0 && ly < H)
                            px[ly * W + lx] = tgt_col;
                    }
                    /* Endpoint dot */
                    sfa_draw_circle(px, W, H, sx1, sy1, 2, tgt_col);
                }
            }
        }
    }

    /* 3D North arrow — yellow line from ship extending in +Z (heading 0) */
    {
        float ny = 0.5f;      /* height above ground plane */
        float nlen = 4.0f;    /* arrow length */
        float nw = 0.03f;     /* line half-width */
        uint32_t n_col = 0xFF00FFFF; /* ABGR yellow */

        /* Shaft: thin vertical quad from ship to ship+Z */
        sr_draw_quad(fb_ptr,
            sr_vert_c(s->x - nw, ny, s->z,        0,0, n_col),
            sr_vert_c(s->x - nw, ny, s->z + nlen, 0,1, n_col),
            sr_vert_c(s->x + nw, ny, s->z + nlen, 1,1, n_col),
            sr_vert_c(s->x + nw, ny, s->z,        1,0, n_col),
            NULL, &vp);
        /* Chevron tip */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(s->x,        ny, s->z + nlen + 0.6f, 0.5f, 0, n_col),
            sr_vert_c(s->x - 0.3f, ny, s->z + nlen,       0,    1, n_col),
            sr_vert_c(s->x + 0.3f, ny, s->z + nlen,       1,    1, n_col),
            NULL, &vp);
    }

    /* 3D Target direction arrow — points from ship toward selected target */
    if (sfa.selected_npc >= 0 && sfa.selected_npc < sfa.npc_count &&
        sfa.npcs[sfa.selected_npc].alive) {
        sfa_ship *tgt = &sfa.npcs[sfa.selected_npc];
        float dx = tgt->x - s->x;
        float dz = tgt->z - s->z;
        float dist = sqrtf(dx * dx + dz * dz);
        if (dist > 0.1f) {
            float ux = dx / dist, uz = dz / dist;  /* unit direction */
            float ty = 0.5f;
            float tlen = 4.0f;
            float tw = 0.03f;
            uint32_t t_col = 0xFFFF6464; /* selected target color */

            /* Perpendicular for line width */
            float px2 = -uz * tw, pz = ux * tw;

            /* Shaft: thin quad from ship toward target */
            sr_draw_quad(fb_ptr,
                sr_vert_c(s->x + px2,            ty, s->z + pz,            0,0, t_col),
                sr_vert_c(s->x + ux*tlen + px2,  ty, s->z + uz*tlen + pz,  0,1, t_col),
                sr_vert_c(s->x + ux*tlen - px2,  ty, s->z + uz*tlen - pz,  1,1, t_col),
                sr_vert_c(s->x - px2,            ty, s->z - pz,            1,0, t_col),
                NULL, &vp);
            /* Chevron tip */
            sr_draw_triangle(fb_ptr,
                sr_vert_c(s->x + ux*(tlen+0.6f),       ty, s->z + uz*(tlen+0.6f),       0.5f, 0, t_col),
                sr_vert_c(s->x + ux*tlen - uz*0.3f,    ty, s->z + uz*tlen + ux*0.3f,    0,    1, t_col),
                sr_vert_c(s->x + ux*tlen + uz*0.3f,    ty, s->z + uz*tlen - ux*0.3f,    1,    1, t_col),
                NULL, &vp);
        }
    }

    /* Draw NPC ships (skip dead) */
    for (int i = 0; i < sfa.npc_count; i++) {
        if (!sfa.npcs[i].alive) continue;
        sfa_draw_target_ship(fb_ptr, &vp, sfa.npcs[i].x, sfa.npcs[i].z,
                             sfa.npcs[i].visual_heading, sfa.npcs[i].ship_class,
                             sfa.npcs[i].is_boss);
    }

    /* Draw phaser beams (2D dithered lines projected to screen) */
    for (int bi = 0; bi < SFA_MAX_BEAMS; bi++) {
        sfa_beam *b = &sfa.beams[bi];
        if (!b->active) continue;

        /* Project both endpoints to screen */
        int sx0, sy0, sx1, sy1;
        float sw0, sw1;
        if (!sfa_project_to_screen(&vp, b->x0, 0.3f, b->z0, W, H, &sx0, &sy0, &sw0))
            continue;
        if (!sfa_project_to_screen(&vp, b->x1, 0.3f, b->z1, W, H, &sx1, &sy1, &sw1))
            continue;

        /* Bresenham-style line with dither */
        int dx = sx1 - sx0;
        int dy = sy1 - sy0;
        int adx = dx < 0 ? -dx : dx;
        int ady = dy < 0 ? -dy : dy;
        int steps = adx > ady ? adx : ady;
        if (steps < 1) steps = 1;

        /* Fade factor based on remaining time */
        float fade = b->timer / SFA_PHASER_BEAM_TIME;

        for (int j = 0; j <= steps; j++) {
            int lx = sx0 + dx * j / steps;
            int ly = sy0 + dy * j / steps;

            /* 2x2 Bayer dither pattern for beam thickness + fade */
            for (int oy = -1; oy <= 1; oy++) {
                for (int ox = -1; ox <= 1; ox++) {
                    int px2 = lx + ox;
                    int py2 = ly + oy;
                    if (px2 < 0 || px2 >= W || py2 < 0 || py2 >= H) continue;
                    /* Dither: checkerboard thins the beam, fades over time */
                    int dist = ox * ox + oy * oy;
                    float threshold = (float)dist * 0.3f + (1.0f - fade) * 1.5f;
                    if (((px2 + py2) & 1) && threshold > 0.8f) continue;
                    if (dist > 1 && threshold > 0.5f) continue;
                    px[py2 * W + px2] = b->color;
                }
            }
        }
    }

    /* Draw torpedoes (2D projected spike starburst — Star Trek style) */
    for (int i = 0; i < SFA_MAX_TORPS; i++) {
        sfa_torpedo *tp = &sfa.torpedoes[i];
        if (!tp->active) continue;

        int tx, ty; float tw;
        if (!sfa_project_to_screen(&vp, tp->x, 0.3f, tp->z, W, H, &tx, &ty, &tw))
            continue;

        /* Pulsing size based on time */
        float pulse = 1.0f + 0.3f * sinf(tp->timer * 15.0f);
        int core_r = (int)(2.0f * pulse);
        int spike_len = (int)(7.0f * pulse);

        /* Core glow — bright center dot */
        sfa_draw_circle(px, W, H, tx, ty, core_r, 0xFFFFFFFF);

        /* Decompose torpedo color for dimmer spike tips */
        uint32_t col = tp->color;
        uint8_t cr = col & 0xFF, cg = (col >> 8) & 0xFF, cb = (col >> 16) & 0xFF;
        uint32_t dim_col = 0xFF000000 | ((cb/2) << 16) | ((cg/2) << 8) | (cr/2);

        /* 6 spikes at 60-degree intervals (lens flare / starburst) */
        for (int s = 0; s < 6; s++) {
            float ang = s * (SFA_PI / 3.0f) + tp->timer * 2.0f; /* slow rotation */
            float cs = cosf(ang), sn = sinf(ang);
            int sx1 = tx + (int)(spike_len * cs);
            int sy1 = ty + (int)(spike_len * sn);

            /* Draw spike line with dithered fade */
            int sdx = sx1 - tx, sdy = sy1 - ty;
            int steps = (sdx < 0 ? -sdx : sdx) > (sdy < 0 ? -sdy : sdy)
                      ? (sdx < 0 ? -sdx : sdx) : (sdy < 0 ? -sdy : sdy);
            if (steps < 1) steps = 1;
            for (int j = 0; j <= steps; j++) {
                int lx = tx + sdx * j / steps;
                int ly = ty + sdy * j / steps;
                if (lx < 0 || lx >= W || ly < 0 || ly >= H) continue;
                /* Dither: skip every other pixel near the tip */
                float t = (float)j / (float)steps;
                if (t > 0.5f && ((lx + ly) & 1)) continue;
                px[ly * W + lx] = (t < 0.4f) ? col : dim_col;
            }
        }
    }

    /* Draw explosions (2D circles projected to screen) */
    for (int i = 0; i < SFA_MAX_EXPLOSIONS; i++) {
        sfa_explosion *e = &sfa.explosions[i];
        if (!e->active) continue;

        int ex, ey; float ew;
        if (!sfa_project_to_screen(&vp, e->x, 0.3f, e->z, W, H, &ex, &ey, &ew))
            continue;

        float t = 1.0f - (e->timer / e->max_timer);
        int radius = 2 + (int)(t * 8.0f);
        sfa_draw_circle(px, W, H, ex, ey, radius, e->color);
    }

    /* Draw targeting brackets on NPC ships */
    {
        for (int i = 0; i < sfa.npc_count; i++) {
            sfa_ship *npc = &sfa.npcs[i];
            int scr_x, scr_y;
            float scr_w;
            if (!sfa_project_to_screen(&vp, npc->x, 0.2f, npc->z, W, H, &scr_x, &scr_y, &scr_w))
                continue;

            float ddx = npc->x - s->x;
            float ddz = npc->z - s->z;
            float dist = sqrtf(ddx*ddx + ddz*ddz);
            int bracket_half = sfa_ship_screen_extent(&vp, npc->x, npc->z,
                                                        npc->visual_heading, W, H,
                                                        npc->ship_class);

            if (!npc->alive) {
                /* Dead: show dim X marker */
                sr_draw_text_shadow(px, W, H, scr_x - 3, scr_y - 4,
                                     "X", 0xFF444444, SFA_HUD_SHADOW);
                continue;
            }

            /* Health bar above brackets — shows hull integrity */
            {
                int hbar_w = bracket_half * 2;
                int hbar_h = 2;
                int hx = scr_x - bracket_half;
                int hy = scr_y - bracket_half - 5;
                int ncls = npc->ship_class;
                if (ncls < 0 || ncls >= SHIP_CLASS_COUNT) ncls = SHIP_CLASS_DESTROYER;
                const ship_class_stats *nsc = &ship_classes[ncls];
                float hp_pct = npc->hull / (float)nsc->hull_max;
                if (hp_pct < 0) hp_pct = 0;
                if (hp_pct > 1) hp_pct = 1;
                int fill = (int)(hbar_w * hp_pct);
                uint32_t hp_col = hp_pct > 0.6f ? 0xFF44CC44
                                : hp_pct > 0.3f ? 0xFF44CCCC
                                :                  0xFF4444CC;
                sfa_draw_rect(px, W, H, hx, hy, hx + hbar_w, hy + hbar_h, 0xC0000000);
                if (fill > 0)
                    sfa_draw_rect(px, W, H, hx, hy, hx + fill, hy + hbar_h, hp_col);
            }

            uint32_t bracket_col;
            if (i == sfa.selected_npc) {
                bracket_col = 0xFFFF6464;
                sfa_draw_targeting_brackets(px, W, H, scr_x, scr_y, bracket_half + 2, bracket_col);
                sfa_draw_targeting_brackets(px, W, H, scr_x, scr_y, bracket_half, bracket_col);

                char tbuf[32];
                snprintf(tbuf, sizeof(tbuf), "TGT: %.0fm", dist);
                sr_draw_text_shadow(px, W, H, scr_x - 18, scr_y + bracket_half + 4,
                                     tbuf, bracket_col, SFA_HUD_SHADOW);
            } else if (i == sfa.hovered_npc) {
                bracket_col = 0xFFFFEE88;
                sfa_draw_targeting_brackets(px, W, H, scr_x, scr_y, bracket_half, bracket_col);
                sr_draw_text_shadow(px, W, H, scr_x - 12, scr_y + bracket_half + 4,
                                     "CLICK", 0xFFCCCCCC, SFA_HUD_SHADOW);
            } else {
                bracket_col = 0xFF555555;
                sfa_draw_targeting_brackets(px, W, H, scr_x, scr_y, bracket_half, bracket_col);
            }
        }
    }

    /* Draw weapon firing arc when hovering over weapon bar */
    if (sfa.hovered_weapon >= 0) {
        float arc_half = (sfa.hovered_weapon == 0) ? SFA_PHASER_ARC : SFA_TORP_ARC;
        float arc_range = (sfa.hovered_weapon == 0) ? SFA_PHASER_RANGE : SFA_TORP_RANGE;
        float arc_vis_r = arc_range * 0.4f; /* visual radius on ground */
        int arc_steps = 24;

        /* Color: green=ready, yellow=charging but in arc/range, red=can't */
        int arc_state = 0; /* 0=red, 1=yellow, 2=green */
        if (sfa.selected_npc >= 0 && sfa.selected_npc < sfa.npc_count) {
            sfa_ship *tgt = &sfa.npcs[sfa.selected_npc];
            if (tgt->alive) {
                float tdx = tgt->x - s->x, tdz = tgt->z - s->z;
                float tdist = sqrtf(tdx * tdx + tdz * tdz);
                bool in_range = tdist <= arc_range;
                bool in_arc = sfa_in_weapon_arc(s, tgt->x, tgt->z, arc_half);
                bool ready = (sfa.hovered_weapon == 0)
                    ? s->phaser_cooldown <= 0
                    : (s->torpedo_cooldown <= 0 && s->torpedoes_remaining > 0);
                if (in_range && in_arc) {
                    arc_state = ready ? 2 : 1;
                }
            }
        }
        uint32_t arc_col = arc_state == 2 ? 0xFF44FF44   /* green */
                         : arc_state == 1 ? 0xFF44CCCC   /* yellow */
                         :                  0xFF4444CC;   /* red */

        /* Draw arc edges (two lines from ship to arc boundary) */
        for (int side = -1; side <= 1; side += 2) {
            float edge_ang = s->heading + arc_half * side;
            float ex = s->x - sinf(edge_ang) * arc_vis_r;
            float ez = s->z + cosf(edge_ang) * arc_vis_r;
            int sx0, sy0, sx1, sy1; float sw0, sw1;
            if (sfa_project_to_screen(&vp, s->x, 0.3f, s->z, W, H, &sx0, &sy0, &sw0) &&
                sfa_project_to_screen(&vp, ex, 0.3f, ez, W, H, &sx1, &sy1, &sw1)) {
                int ldx = sx1 - sx0, ldy = sy1 - sy0;
                int steps = (ldx<0?-ldx:ldx) > (ldy<0?-ldy:ldy) ? (ldx<0?-ldx:ldx) : (ldy<0?-ldy:ldy);
                if (steps < 1) steps = 1;
                for (int j = 0; j <= steps; j++) {
                    int lx = sx0 + ldx * j / steps;
                    int ly = sy0 + ldy * j / steps;
                    if (lx >= 0 && lx < W && ly >= 0 && ly < H) {
                        if ((lx + ly) & 1) continue; /* dither */
                        px[ly * W + lx] = arc_col;
                    }
                }
            }
        }

        /* Draw arc curve between the two edges */
        float a_start = s->heading - arc_half;
        for (int i = 0; i < arc_steps; i++) {
            float a0 = a_start + (2.0f * arc_half) * (float)i / arc_steps;
            float a1 = a_start + (2.0f * arc_half) * (float)(i + 1) / arc_steps;
            float wx0 = s->x - sinf(a0) * arc_vis_r;
            float wz0 = s->z + cosf(a0) * arc_vis_r;
            float wx1 = s->x - sinf(a1) * arc_vis_r;
            float wz1 = s->z + cosf(a1) * arc_vis_r;
            int sx0, sy0, sx1, sy1; float sw0, sw1;
            if (!sfa_project_to_screen(&vp, wx0, 0.3f, wz0, W, H, &sx0, &sy0, &sw0)) continue;
            if (!sfa_project_to_screen(&vp, wx1, 0.3f, wz1, W, H, &sx1, &sy1, &sw1)) continue;
            int ldx = sx1 - sx0, ldy = sy1 - sy0;
            int steps = (ldx<0?-ldx:ldx) > (ldy<0?-ldy:ldy) ? (ldx<0?-ldx:ldx) : (ldy<0?-ldy:ldy);
            if (steps < 1) steps = 1;
            for (int j = 0; j <= steps; j++) {
                int lx = sx0 + ldx * j / steps;
                int ly = sy0 + ldy * j / steps;
                if (lx >= 0 && lx < W && ly >= 0 && ly < H) {
                    if ((lx + ly) & 1) continue;
                    px[ly * W + lx] = arc_col;
                }
            }
        }
    }

    /* Draw HUD */
    sfa_draw_hud(fb_ptr, s);

    /* Victory overlay (drawn on top of everything) */
    if (sfa.phase == SFA_PHASE_VICTORY) {
        sfa_draw_victory_overlay(px, W, H);
    }
}

#endif /* SFA_SCENE_H */
