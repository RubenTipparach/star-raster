/*  sr_app.h — Shared application state, constants, and enums.
 *  Included by all scene/input modules. Single-TU header-only. */
#ifndef SR_APP_H
#define SR_APP_H

#include <stdio.h>
#include <math.h>
#include <time.h>

/* ── Configuration ───────────────────────────────────────────────── */

#define FB_WIDTH       480
#define FB_HEIGHT      270
#define TARGET_FPS     60
#define GIF_TARGET_FPS 24.0
#define FOG_NEAR       35.0f
#define FOG_FAR        60.0f
#define FOG_COLOR      0xFFEBCE87

/* ── Globals ─────────────────────────────────────────────────────── */

static sr_framebuffer fb;

/* Texture enums */
enum {
    TEX_BRICK, TEX_GRASS, TEX_ROOF, TEX_WOOD, TEX_TILE, TEX_COUNT
};
static sr_texture textures[TEX_COUNT];

/* Indexed (palette) textures */
enum {
    ITEX_BRICK, ITEX_GRASS, ITEX_ROOF, ITEX_WOOD, ITEX_TILE, ITEX_STONE, ITEX_COUNT
};
static sr_indexed_texture itextures[ITEX_COUNT];

/* Timing */
static double time_acc;
static int    frame_counter;
static double fps_timer;
static int    fps_frame_count;
static int    fps_display;
static double gif_capture_timer;
static int    screenshot_counter;

/* ── Scene / Menu state ─────────────────────────────────────────── */

enum { SCENE_NEIGHBORHOOD, SCENE_CUBES, SCENE_PALETTE_HOUSE, SCENE_DUNGEON, SCENE_COUNT };
enum { STATE_MENU, STATE_RUNNING };

static int  app_state     = STATE_MENU;
static int  current_scene = SCENE_NEIGHBORHOOD;
static int  menu_cursor   = 0;

static const char *scene_names[] = {
    "NEIGHBORHOOD",
    "5000 CUBES",
    "PALETTE HOUSE",
    "DUNGEON CRAWLER",
};

/* ── Simple RNG (deterministic) ─────────────────────────────────── */

static uint32_t rng_state = 12345;
static float rng_float(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (float)((rng_state >> 16) & 0x7FFF) / 32768.0f;
}
static float rng_range(float lo, float hi) {
    return lo + rng_float() * (hi - lo);
}

/* ── Coordinate mapping ─────────────────────────────────────────── */

static void screen_to_fb(float sx, float sy, float *fbx, float *fby) {
    float fb_aspect  = (float)FB_WIDTH / (float)FB_HEIGHT;
    float win_w = sapp_widthf(), win_h = sapp_heightf();
    float win_aspect = win_w / win_h;

    float scaled_w, scaled_h;
    if (win_aspect > fb_aspect) {
        scaled_h = win_h;
        scaled_w = win_h * fb_aspect;
    } else {
        scaled_w = win_w;
        scaled_h = win_w / fb_aspect;
    }

    float ox = (win_w - scaled_w) * 0.5f;
    float oy = (win_h - scaled_h) * 0.5f;

    *fbx = (sx - ox) / scaled_w * (float)FB_WIDTH;
    *fby = (sy - oy) / scaled_h * (float)FB_HEIGHT;
}

#endif /* SR_APP_H */
