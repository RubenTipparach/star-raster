/*  sfa_input.h — Touch, mouse, and keyboard input handlers. Header-only. Depends on sfa_types.h, sfa_combat.h, sfa_render.h, sfa_hud.h. */
#ifndef SFA_INPUT_H
#define SFA_INPUT_H

/* ── Touch input handling ────────────────────────────────────────── */

static bool sfa_handle_touch_began(float sx, float sy) {
    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);

    /* Phase transitions on click */
    if (sfa.phase == SFA_PHASE_BRIEFING) {
        sfa.phase = SFA_PHASE_COMBAT;
        return true;
    }
    if (sfa.phase == SFA_PHASE_STATS) {
        sfa.initialized = false;  /* reset for next play */
        app_state = STATE_MENU;
        return true;
    }
    if (sfa.phase == SFA_PHASE_VICTORY) {
        return true;  /* absorb clicks during victory countdown */
    }

    /* Check top-right buttons */
    {
        int mby = 3, mbh = 11;

        /* MENU button */
        int menu_x = FB_WIDTH - 32;
        if (fx >= menu_x && fx <= menu_x + 30 && fy >= mby && fy <= mby + mbh) {
            app_state = STATE_MENU;
            return true;
        }

        /* CLR TGT button */
        int clr_x = menu_x - 50;
        if (fx >= clr_x && fx <= clr_x + 48 && fy >= mby && fy <= mby + mbh) {
            sfa.selected_npc = -1;
            return true;
        }

        /* SRS/LRS toggle button */
        int sns_x = clr_x - 32;
        if (fx >= sns_x && fx <= sns_x + 30 && fy >= mby && fy <= mby + mbh) {
            sfa.long_range_sensors = !sfa.long_range_sensors;
            return true;
        }
    }

    /* Check turn buttons (top-left = turn left, top-right = turn right) */
    {
        int lx = SFA_TURN_BTN_MARGIN;
        int rx = FB_WIDTH - SFA_TURN_BTN_W - SFA_TURN_BTN_MARGIN;
        int ty = SFA_TURN_BTN_Y;
        if (fx >= lx && fx <= lx + SFA_TURN_BTN_W &&
            fy >= ty && fy <= ty + SFA_TURN_BTN_H) {
            sfa.touch_turn_left = true;
            return true;
        }
        if (fx >= rx && fx <= rx + SFA_TURN_BTN_W &&
            fy >= ty && fy <= ty + SFA_TURN_BTN_H) {
            sfa.touch_turn_right = true;
            return true;
        }
    }

    /* Check minimap click — select target by clicking its dot */
    {
        int scx = SFA_VCTRL_STEER_CX;
        int scy = SFA_VCTRL_STEER_CY;
        int sr = SFA_VCTRL_STEER_R;
        float mdx = fx - scx, mdy = fy - scy;
        if (mdx * mdx + mdy * mdy < (float)(sr * sr)) {
            float max_range = sfa.long_range_sensors ? SFA_SENSOR_LONG : SFA_SENSOR_SHORT;
            float map_r = (float)(sr - 4);
            int best = -1;
            float best_d = 10.0f; /* pixel threshold */
            for (int i = 0; i < sfa.npc_count; i++) {
                if (!sfa.npcs[i].alive) continue;
                float ddx = sfa.npcs[i].x - sfa.player.x;
                float ddz = sfa.npcs[i].z - sfa.player.z;
                float dist = sqrtf(ddx * ddx + ddz * ddz);
                /* Unit direction * clamped distance (no rotation, X negated to match dial) */
                float r = (dist < 0.01f) ? 0.0f : fminf(dist / max_range, 1.0f) * map_r;
                float ux = (dist < 0.01f) ? 0.0f : ddx / dist;
                float uz = (dist < 0.01f) ? 0.0f : ddz / dist;
                int ex = scx - (int)(ux * r);  /* minimap right = world -X */
                int ey = scy - (int)(uz * r);
                float pd = sqrtf((fx - ex) * (fx - ex) + (fy - ey) * (fy - ey));
                if (pd < best_d) { best_d = pd; best = i; }
            }
            if (best >= 0) {
                sfa.selected_npc = best;
                return true;
            }
        }
    }

    /* Check weapon fire bars (bottom-center, fixed size) */
    {
        int total_w = SFA_WBAR_W * 2 + SFA_WBAR_GAP;
        int wx0 = (FB_WIDTH - total_w) / 2;
        int wy = FB_HEIGHT - SFA_WBAR_H - 4;

        /* PHASER bar */
        if (fx >= wx0 && fx <= wx0 + SFA_WBAR_W && fy >= wy && fy <= wy + SFA_WBAR_H) {
            if (sfa.selected_npc >= 0 && sfa.selected_npc < sfa.npc_count) {
                sfa_ship *tgt = &sfa.npcs[sfa.selected_npc];
                if (tgt->alive) sfa_fire_phaser(&sfa.player, tgt, -1, sfa.selected_npc);
            }
            return true;
        }
        /* TORP bar */
        int tx = wx0 + SFA_WBAR_W + SFA_WBAR_GAP;
        if (fx >= tx && fx <= tx + SFA_WBAR_W && fy >= wy && fy <= wy + SFA_WBAR_H) {
            if (sfa.selected_npc >= 0 && sfa.selected_npc < sfa.npc_count) {
                sfa_ship *tgt = &sfa.npcs[sfa.selected_npc];
                if (tgt->alive)
                    sfa_fire_torpedo(&sfa.player, tgt, -1, sfa.selected_npc);
            }
            return true;
        }
    }

    /* Check throttle buttons */
    int bx = SFA_VCTRL_BTN_X;
    int by_base = FB_HEIGHT - 26 - (SFA_NUM_SPEEDS * (SFA_VCTRL_BTN_H + SFA_VCTRL_BTN_GAP));
    for (int i = SFA_NUM_SPEEDS - 1; i >= 0; i--) {
        int idx = SFA_NUM_SPEEDS - 1 - i;
        int by = by_base + idx * (SFA_VCTRL_BTN_H + SFA_VCTRL_BTN_GAP);
        if (fx >= bx && fx <= bx + SFA_VCTRL_BTN_W &&
            fy >= by && fy <= by + SFA_VCTRL_BTN_H) {
            sfa.player.speed_level = i;
            return true;
        }
    }

    /* Plane-cast steering: click near the ship in 3D to set heading (last) */
    if (sfa.last_fb_w > 0) {
        float gx, gz;
        if (sfa_screen_to_ground(fx, fy, sfa.last_fb_w, sfa.last_fb_h,
                                  &sfa.last_inv_vp, sfa.last_eye, &gx, &gz)) {
            float pdx = gx - sfa.player.x;
            float pdz = gz - sfa.player.z;
            float pdist = sqrtf(pdx * pdx + pdz * pdz);
            if (pdist > 1.0f && pdist < SFA_STEER_DISC_R) {
                sfa.touch_steering = true;
                float angle = atan2f(-pdx, pdz);
                sfa.touch_steer_angle = angle;
                sfa.player.target_heading = angle;
                return true;
            }
        }
    }

    return false;
}

static void sfa_handle_touch_moved(float sx, float sy) {
    if (!sfa.touch_steering) return;
    if (sfa.last_fb_w <= 0) return;

    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);

    float gx, gz;
    if (sfa_screen_to_ground(fx, fy, sfa.last_fb_w, sfa.last_fb_h,
                              &sfa.last_inv_vp, sfa.last_eye, &gx, &gz)) {
        float pdx = gx - sfa.player.x;
        float pdz = gz - sfa.player.z;
        float pdist = sqrtf(pdx * pdx + pdz * pdz);
        if (pdist > 1.0f) {
            float angle = atan2f(-pdx, pdz);
            sfa.touch_steer_angle = angle;
            sfa.player.target_heading = angle;
        }
    }
}

static void sfa_handle_touch_ended(void) {
    sfa.touch_steering = false;
    sfa.touch_throttle = false;
    sfa.touch_turn_left = false;
    sfa.touch_turn_right = false;
}

/* ── Mouse input for targeting ────────────────────────────────────── */

static void sfa_handle_mouse_move(float sx, float sy) {
    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);
    sfa.mouse_fb_x = fx;
    sfa.mouse_fb_y = fy;

    /* Update hover using cached VP matrix from last frame */
    if (sfa.last_fb_w > 0)
        sfa_update_hover(&sfa.last_vp, sfa.last_fb_w, sfa.last_fb_h);

    /* Detect weapon bar hover */
    sfa.hovered_weapon = -1;
    {
        int bar_h = 20, gap = 4, pad = 4;
        char ph_l[20], tp_l[20];
        if (sfa.player.phaser_cooldown <= 0)
            snprintf(ph_l, sizeof(ph_l), "PHSR [SPC]");
        else
            snprintf(ph_l, sizeof(ph_l), "PHSR %.1f", sfa.player.phaser_cooldown);
        if (sfa.player.torpedoes_remaining <= 0)
            snprintf(tp_l, sizeof(tp_l), "TORP EMPTY");
        else if (sfa.player.torpedo_cooldown <= 0)
            snprintf(tp_l, sizeof(tp_l), "TORP [F] x%d", sfa.player.torpedoes_remaining);
        else
            snprintf(tp_l, sizeof(tp_l), "TORP x%d %.1f", sfa.player.torpedoes_remaining, sfa.player.torpedo_cooldown);
        int ph_w = (int)strlen(ph_l) * 6 + pad * 2;
        int tp_w = (int)strlen(tp_l) * 6 + pad * 2;
        int total_w = ph_w + gap + tp_w;
        int wx0 = (FB_WIDTH - total_w) / 2;
        int wy = FB_HEIGHT - bar_h - 4;

        if (fy >= wy && fy <= wy + bar_h) {
            if (fx >= wx0 && fx <= wx0 + ph_w)
                sfa.hovered_weapon = 0; /* phaser */
            else if (fx >= wx0 + ph_w + gap && fx <= wx0 + ph_w + gap + tp_w)
                sfa.hovered_weapon = 1; /* torpedo */
        }
    }
}

static bool sfa_handle_mouse_click(float sx, float sy) {
    /* Phase transitions handled by touch_began */
    if (sfa.phase != SFA_PHASE_COMBAT) return false;

    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);
    sfa.mouse_fb_x = fx;
    sfa.mouse_fb_y = fy;

    /* Re-run hover detection with cached VP so we have fresh hovered_npc */
    if (sfa.last_fb_w > 0)
        sfa_update_hover(&sfa.last_vp, sfa.last_fb_w, sfa.last_fb_h);

    /* If hovering an NPC, select it (use CLR TGT button to deselect) */
    if (sfa.hovered_npc >= 0) {
        sfa.selected_npc = sfa.hovered_npc;
        return true;  /* consumed — don't pass to steering */
    }

    return false;  /* not consumed — let touch_began handle it */
}

/* ── Key input ───────────────────────────────────────────────────── */

static void sfa_handle_key_down(sapp_keycode key) {
    /* Phase transition keys */
    if (sfa.phase == SFA_PHASE_BRIEFING) {
        if (key == SAPP_KEYCODE_SPACE || key == SAPP_KEYCODE_ENTER) {
            sfa.phase = SFA_PHASE_COMBAT;
        }
        return;
    }
    if (sfa.phase == SFA_PHASE_STATS) {
        if (key == SAPP_KEYCODE_SPACE || key == SAPP_KEYCODE_ENTER ||
            key == SAPP_KEYCODE_ESCAPE) {
            sfa.initialized = false;
            app_state = STATE_MENU;
        }
        return;
    }
    if (sfa.phase == SFA_PHASE_VICTORY) return;

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
        case SAPP_KEYCODE_TAB:
            /* Cycle through targets (skip dead NPCs) */
            if (sfa.npc_count > 0) {
                int start = sfa.selected_npc;
                do {
                    sfa.selected_npc++;
                    if (sfa.selected_npc >= sfa.npc_count) {
                        sfa.selected_npc = -1;
                        break;
                    }
                } while (!sfa.npcs[sfa.selected_npc].alive &&
                         sfa.selected_npc != start);
            }
            break;
        case SAPP_KEYCODE_SPACE:
            /* Fire phasers at selected target */
            if (sfa.selected_npc >= 0 && sfa.selected_npc < sfa.npc_count) {
                sfa_ship *tgt = &sfa.npcs[sfa.selected_npc];
                if (tgt->alive) sfa_fire_phaser(s, tgt, -1, sfa.selected_npc);
            }
            break;
        case SAPP_KEYCODE_F:
            /* Fire torpedo at selected target */
            if (sfa.selected_npc >= 0 && sfa.selected_npc < sfa.npc_count) {
                sfa_ship *tgt = &sfa.npcs[sfa.selected_npc];
                if (tgt->alive)
                    sfa_fire_torpedo(s, tgt, -1, sfa.selected_npc);
            }
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

#endif /* SFA_INPUT_H */
