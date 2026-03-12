/*  sr_mobile_input.h — Touch / swipe input for mobile.
 *  Single-TU header-only. Depends on sr_app.h, sr_scene_dungeon.h. */
#ifndef SR_MOBILE_INPUT_H
#define SR_MOBILE_INPUT_H

/* ── Touch / swipe state ─────────────────────────────────────────── */

static bool   touch_active = false;
static float  touch_start_sx, touch_start_sy;
static double touch_start_time;
static float  touch_cur_sx, touch_cur_sy;

#define TOUCH_TAP_MAX_TIME   0.25
#define TOUCH_SWIPE_MIN_DIST 30.0f


static void dng_touch_began(float sx, float sy, double time) {
    touch_active = true;
    touch_start_sx = sx;
    touch_start_sy = sy;
    touch_cur_sx = sx;
    touch_cur_sy = sy;
    touch_start_time = time;
}

static void dng_touch_moved(float sx, float sy) {
    if (touch_active) {
        touch_cur_sx = sx;
        touch_cur_sy = sy;
    }
}

static void dng_touch_ended(float sx, float sy, double time) {
    if (!touch_active) return;
    touch_active = false;

    if (current_scene != SCENE_DUNGEON || dng_play_state != DNG_STATE_PLAYING) return;

    /* Check MENU button first */
    {
        float fx, fy;
        screen_to_fb(sx, sy, &fx, &fy);
        int mbx = FB_WIDTH - 32, mby = 3, mbw = 30, mbh = 11;
        if (fx >= mbx && fx <= mbx + mbw && fy >= mby && fy <= mby + mbh) {
            app_state = STATE_MENU;
            return;
        }
    }

    float dx = sx - touch_start_sx;
    float dy = sy - touch_start_sy;
    float dist = sqrtf(dx * dx + dy * dy);
    double duration = time - touch_start_time;

    if (dist < TOUCH_SWIPE_MIN_DIST && duration < TOUCH_TAP_MAX_TIME) {
        /* Short tap — strafe based on screen half */
        float mid_x = sapp_widthf() * 0.5f;
        if (sx < mid_x) {
            dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                (dng_state.player.dir + 3) % 4);
        } else {
            dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                (dng_state.player.dir + 1) % 4);
        }
    } else if (dist >= TOUCH_SWIPE_MIN_DIST) {
        /* Swipe — determine cardinal direction */
        float adx = dx < 0 ? -dx : dx;
        float ady = dy < 0 ? -dy : dy;

        if (ady > adx) {
            if (dy < 0) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    dng_state.player.dir);
            } else {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    (dng_state.player.dir + 2) % 4);
            }
        } else {
            if (dx < 0) {
                dng_state.player.dir = (dng_state.player.dir + 3) % 4;
                dng_state.player.target_angle -= 0.25f;
            } else {
                dng_state.player.dir = (dng_state.player.dir + 1) % 4;
                dng_state.player.target_angle += 0.25f;
            }
        }
    }
}

static void dng_touch_cancelled(void) {
    touch_active = false;
}

#endif /* SR_MOBILE_INPUT_H */
