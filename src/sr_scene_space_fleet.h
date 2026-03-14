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
#define SFA_GRID_SPACING 10.0f      /* starfield grid spacing (sparser) */

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

/* Camera — pitched angle, behind and above the ship */
#define SFA_CAM_HEIGHT   18.0f       /* height above ship */
#define SFA_CAM_BACK     22.0f       /* distance behind ship */
#define SFA_CAM_LOOK_AHEAD 8.0f     /* look-at point ahead of ship */

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

/* ── Palette helper ──────────────────────────────────────────────── */

static inline uint32_t sfa_pal_abgr(int idx) {
    uint32_t rgb = pal_colors[idx];
    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >> 8) & 0xFF;
    uint32_t b = rgb & 0xFF;
    return 0xFF000000 | (b << 16) | (g << 8) | r;
}

/* Shade a palette color: shade 0=darkest, PAL_MID_ROW=normal, PAL_SHADES-1=brightest */
static inline uint32_t sfa_pal_shade(int base_idx, int shade) {
    if (shade < 0) shade = 0;
    if (shade >= PAL_SHADES) shade = PAL_SHADES - 1;
    if (base_idx < 0 || base_idx >= PAL_COLORS) base_idx = 64; /* black */
    int shifted = pal_shift_lut[shade][base_idx];
    return sfa_pal_abgr(shifted);
}

/* ── Ship state ──────────────────────────────────────────────────── */

#define SFA_MAX_NPC 4

/* Targeting bracket colors */
#define SFA_TARGET_HOVER    0xFF88AACC   /* dim bracket on hover */
#define SFA_TARGET_SELECTED 0xFFFFDD44   /* bright blue glow (ABGR) when selected */
#define SFA_TARGET_BRACKET_SIZE 2.0f     /* world-space bracket extent */

typedef struct {
    float x, z;               /* world position */
    float heading;            /* current heading in radians (0 = +Z, CW) */
    float target_heading;     /* desired heading */
    int   speed_level;        /* 0-3 */
    float current_speed;      /* actual speed (accelerates toward target) */

    /* Shield facings: F, FR, AR, A, AL, FL (clockwise from front) */
    float shields[6];
    float hull;

    /* Smooth interpolation */
    float visual_heading;     /* smoothed heading for rendering */

    /* Ship identity */
    uint32_t color_hull;      /* custom hull color (0 = use default) */
    uint32_t color_accent;    /* custom accent color */
} sfa_ship;

typedef struct {
    sfa_ship player;
    sfa_ship npcs[SFA_MAX_NPC];
    int      npc_count;
    float    time;
    bool     initialized;

    /* Targeting */
    int      hovered_npc;         /* NPC index under cursor, -1 = none */
    int      selected_npc;        /* NPC index locked on, -1 = none */
    float    mouse_fb_x, mouse_fb_y;  /* mouse pos in fb coords */
    float    cam_target_yaw;      /* smoothed camera yaw toward target */

    /* Touch controls */
    bool     touch_steering;      /* is user dragging to steer? */
    float    touch_steer_cx;      /* center of steering circle (fb coords) */
    float    touch_steer_cy;
    float    touch_steer_angle;   /* current angle from touch */
    bool     touch_throttle;      /* throttle touch active */

    /* Target */
    float    target_x, target_z;  /* target world position */
    bool     target_selected;     /* is the target currently selected? */
    int      target_screen_x;     /* last projected screen position */
    int      target_screen_y;
    bool     target_on_screen;    /* was target visible last frame? */
} sfa_state;

static sfa_state sfa;

/* ── Key held state (for smooth turning) ─────────────────────────── */

static bool sfa_key_left  = false;
static bool sfa_key_right = false;
static bool sfa_key_up    = false;
static bool sfa_key_down  = false;

/* ── Initialization ──────────────────────────────────────────────── */

static void sfa_init_ship(sfa_ship *s, float x, float z, float heading,
                          uint32_t hull_col, uint32_t accent_col) {
    s->x = x;
    s->z = z;
    s->heading = heading;
    s->target_heading = heading;
    s->visual_heading = heading;
    s->speed_level = SFA_SPEED_STOP;
    s->hull = 100.0f;
    s->color_hull = hull_col;
    s->color_accent = accent_col;
    for (int i = 0; i < 6; i++)
        s->shields[i] = 100.0f;
}

static void sfa_init(void) {
    memset(&sfa, 0, sizeof(sfa));

    /* Player ship — default orange */
    sfa_init_ship(&sfa.player, 0.0f, 0.0f, 0.0f, 0, 0);

    /* NPC ship — Klingon green, parked nearby */
    sfa.npc_count = 1;
    sfa_init_ship(&sfa.npcs[0], 15.0f, 20.0f, SFA_PI * 0.75f,
                  0xFF33AA55, 0xFF55DD77);   /* green hull */
    sfa.npcs[0].speed_level = SFA_SPEED_QUARTER;

    sfa.hovered_npc = -1;
    sfa.selected_npc = -1;
    sfa.cam_target_yaw = 0.0f;

    /* Place a target ship in the arena */
    sfa.target_x = 30.0f;
    sfa.target_z = 20.0f;
    sfa.target_selected = false;

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

    /* Accelerate toward target speed */
    float target_speed = sfa_speed_values[s->speed_level];
    float accel = 4.0f; /* units/sec² */
    if (s->current_speed < target_speed) {
        s->current_speed += accel * dt;
        if (s->current_speed > target_speed) s->current_speed = target_speed;
    } else if (s->current_speed > target_speed) {
        s->current_speed -= accel * dt * 1.5f; /* braking is faster */
        if (s->current_speed < target_speed) s->current_speed = target_speed;
    }

    /* Move ship */
    s->x -= sinf(s->heading) * s->current_speed * dt;
    s->z += cosf(s->heading) * s->current_speed * dt;

    /* Clamp to arena bounds */
    if (s->x >  SFA_ARENA_SIZE) s->x =  SFA_ARENA_SIZE;
    if (s->x < -SFA_ARENA_SIZE) s->x = -SFA_ARENA_SIZE;
    if (s->z >  SFA_ARENA_SIZE) s->z =  SFA_ARENA_SIZE;
    if (s->z < -SFA_ARENA_SIZE) s->z = -SFA_ARENA_SIZE;

    /* Update NPC ships (simple patrol — they just fly in circles) */
    for (int i = 0; i < sfa.npc_count; i++) {
        sfa_ship *npc = &sfa.npcs[i];
        /* Slow turn to patrol in a circle */
        npc->target_heading += 0.3f * dt;
        npc->target_heading = sfa_normalize_angle(npc->target_heading);

        float ndiff = sfa_normalize_angle(npc->target_heading - npc->heading);
        float nmax = SFA_TURN_RATE * dt;
        if (ndiff > nmax) ndiff = nmax;
        else if (ndiff < -nmax) ndiff = -nmax;
        npc->heading += ndiff;
        npc->heading = sfa_normalize_angle(npc->heading);

        float nvdiff = sfa_normalize_angle(npc->heading - npc->visual_heading);
        npc->visual_heading += nvdiff * 8.0f * dt;
        npc->visual_heading = sfa_normalize_angle(npc->visual_heading);

        float nspeed = sfa_speed_values[npc->speed_level];
        npc->x += sinf(npc->heading) * nspeed * dt;
        npc->z += cosf(npc->heading) * nspeed * dt;

        /* Clamp NPC to arena */
        if (npc->x >  SFA_ARENA_SIZE) npc->x =  SFA_ARENA_SIZE;
        if (npc->x < -SFA_ARENA_SIZE) npc->x = -SFA_ARENA_SIZE;
        if (npc->z >  SFA_ARENA_SIZE) npc->z =  SFA_ARENA_SIZE;
        if (npc->z < -SFA_ARENA_SIZE) npc->z = -SFA_ARENA_SIZE;
    }

    /* Smooth camera yaw toward selected target */
    if (sfa.selected_npc >= 0 && sfa.selected_npc < sfa.npc_count) {
        sfa_ship *tgt = &sfa.npcs[sfa.selected_npc];
        float dx = tgt->x - s->x;
        float dz = tgt->z - s->z;
        float angle_to_target = atan2f(dx, dz);
        /* Blend between ship heading and target direction */
        float cam_diff = sfa_normalize_angle(angle_to_target - sfa.cam_target_yaw);
        sfa.cam_target_yaw += cam_diff * 3.0f * dt;
        sfa.cam_target_yaw = sfa_normalize_angle(sfa.cam_target_yaw);
    } else {
        /* No target — follow ship heading */
        float cam_diff = sfa_normalize_angle(s->visual_heading - sfa.cam_target_yaw);
        sfa.cam_target_yaw += cam_diff * 5.0f * dt;
        sfa.cam_target_yaw = sfa_normalize_angle(sfa.cam_target_yaw);
    }
}

/* ── 3D Ship model ───────────────────────────────────────────────── */
/* Federation-style: saucer section (front), engineering hull (rear),
 * two nacelles on pylons. All geometry is 3D with proper Y height. */

static void sfa_draw_box(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                          float x0, float y0, float z0,
                          float x1, float y1, float z1,
                          uint32_t top_col, uint32_t side_col, uint32_t bottom_col) {
    /* Top (+Y) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x0, y1, z1, 0,0, top_col), sr_vert_c(x0, y1, z0, 0,1, top_col),
        sr_vert_c(x1, y1, z0, 1,1, top_col), sr_vert_c(x1, y1, z1, 1,0, top_col),
        NULL, mvp);
    /* Bottom (-Y) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x0, y0, z0, 0,0, bottom_col), sr_vert_c(x0, y0, z1, 0,1, bottom_col),
        sr_vert_c(x1, y0, z1, 1,1, bottom_col), sr_vert_c(x1, y0, z0, 1,0, bottom_col),
        NULL, mvp);
    /* Front (+Z) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x0, y0, z1, 0,0, side_col), sr_vert_c(x0, y1, z1, 0,1, side_col),
        sr_vert_c(x1, y1, z1, 1,1, side_col), sr_vert_c(x1, y0, z1, 1,0, side_col),
        NULL, mvp);
    /* Back (-Z) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x1, y0, z0, 0,0, side_col), sr_vert_c(x1, y1, z0, 0,1, side_col),
        sr_vert_c(x0, y1, z0, 1,1, side_col), sr_vert_c(x0, y0, z0, 1,0, side_col),
        NULL, mvp);
    /* Left (-X) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x0, y0, z0, 0,0, side_col), sr_vert_c(x0, y1, z0, 0,1, side_col),
        sr_vert_c(x0, y1, z1, 1,1, side_col), sr_vert_c(x0, y0, z1, 1,0, side_col),
        NULL, mvp);
    /* Right (+X) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(x1, y0, z1, 0,0, side_col), sr_vert_c(x1, y1, z1, 0,1, side_col),
        sr_vert_c(x1, y1, z0, 1,1, side_col), sr_vert_c(x1, y0, z0, 1,0, side_col),
        NULL, mvp);
}

static void sfa_draw_ship(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                           sfa_ship *s) {
    float h = s->visual_heading;

    sr_mat4 model = sr_mat4_mul(
        sr_mat4_translate(s->x, 0.0f, s->z),
        sr_mat4_rotate_y(-h)
    );
    sr_mat4 mvp = sr_mat4_mul(*vp, model);

    /* Colors — use per-ship overrides if set */
    uint32_t hull_top   = s->color_hull   ? s->color_hull   : SFA_SHIP_COLOR;
    uint32_t nacelle_t  = s->color_accent ? s->color_accent : SFA_SHIP_ACCENT;
    /* Derive darker shades from hull color */
    uint8_t hr = (uint8_t)(hull_top & 0xFF);
    uint8_t hg = (uint8_t)((hull_top >> 8) & 0xFF);
    uint8_t hb = (uint8_t)((hull_top >> 16) & 0xFF);
    uint32_t hull_side = 0xFF000000 | ((uint32_t)(hb*3/4)<<16) | ((uint32_t)(hg*3/4)<<8) | (hr*3/4);
    uint32_t hull_bot  = 0xFF000000 | ((uint32_t)(hb/2)<<16) | ((uint32_t)(hg/2)<<8) | (hr/2);
    uint8_t nr = (uint8_t)(nacelle_t & 0xFF);
    uint8_t ng = (uint8_t)((nacelle_t >> 8) & 0xFF);
    uint8_t nb = (uint8_t)((nacelle_t >> 16) & 0xFF);
    uint32_t nacelle_s = 0xFF000000 | ((uint32_t)(nb*3/4)<<16) | ((uint32_t)(ng*3/4)<<8) | (nr*3/4);
    uint32_t nacelle_b = 0xFF000000 | ((uint32_t)(nb/2)<<16) | ((uint32_t)(ng/2)<<8) | (nr/2);
    uint32_t pylon_col  = 0xFF887755;
    uint32_t bridge_col = 0xFFFFDDAA;
    uint32_t deflector  = 0xFFFFAA33;  /* blue-ish glow (ABGR) */

    /* ── Saucer section (front disc approximated as octagonal prism) ── */
    {
        float sr = 0.9f;    /* saucer radius */
        float sh = 0.12f;   /* saucer half-height */
        float sy = 0.15f;   /* saucer Y center */
        float sz = 0.6f;    /* saucer Z center (forward) */
        int n = 8;
        float angles[9]; /* n+1 for closing */
        for (int i = 0; i <= n; i++)
            angles[i] = SFA_TWO_PI * (float)i / (float)n;

        for (int i = 0; i < n; i++) {
            float c0 = cosf(angles[i]),   s0 = sinf(angles[i]);
            float c1 = cosf(angles[i+1]), s1 = sinf(angles[i+1]);
            float x0 = s0 * sr, z0 = c0 * sr + sz;
            float x1 = s1 * sr, z1 = c1 * sr + sz;

            /* Top face wedge */
            sr_draw_triangle(fb_ptr,
                sr_vert_c(0, sy + sh, sz, 0.5f, 0.5f, hull_top),
                sr_vert_c(x0, sy + sh, z0, 0, 0, hull_top),
                sr_vert_c(x1, sy + sh, z1, 1, 0, hull_top),
                NULL, &mvp);
            /* Bottom face wedge */
            sr_draw_triangle(fb_ptr,
                sr_vert_c(0, sy - sh, sz, 0.5f, 0.5f, hull_bot),
                sr_vert_c(x1, sy - sh, z1, 1, 0, hull_bot),
                sr_vert_c(x0, sy - sh, z0, 0, 0, hull_bot),
                NULL, &mvp);
            /* Side rim */
            sr_draw_quad(fb_ptr,
                sr_vert_c(x0, sy - sh, z0, 0, 0, hull_side),
                sr_vert_c(x1, sy - sh, z1, 1, 0, hull_side),
                sr_vert_c(x1, sy + sh, z1, 1, 1, hull_side),
                sr_vert_c(x0, sy + sh, z0, 0, 1, hull_side),
                NULL, &mvp);
        }

        /* Bridge dome (small raised box on top of saucer) */
        sfa_draw_box(fb_ptr, &mvp,
                     -0.15f, sy + sh, sz - 0.15f,
                      0.15f, sy + sh + 0.12f, sz + 0.15f,
                     bridge_col, bridge_col, hull_top);
    }

    /* ── Engineering hull (rear section — elongated box) ── */
    {
        float ew = 0.25f;  /* half-width */
        float eh = 0.2f;   /* half-height */
        float ey = 0.0f;   /* Y center */
        float ez0 = -1.4f; /* aft end */
        float ez1 = 0.3f;  /* connects to saucer */

        sfa_draw_box(fb_ptr, &mvp,
                     -ew, ey - eh, ez0,
                      ew, ey + eh, ez1,
                     hull_top, hull_side, hull_bot);

        /* Deflector dish (front face of engineering hull) */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-ew*0.6f, ey - eh*0.6f, ez1 + 0.01f, 0,0, deflector),
            sr_vert_c(-ew*0.6f, ey + eh*0.6f, ez1 + 0.01f, 0,1, deflector),
            sr_vert_c( ew*0.6f, ey + eh*0.6f, ez1 + 0.01f, 1,1, deflector),
            sr_vert_c( ew*0.6f, ey - eh*0.6f, ez1 + 0.01f, 1,0, deflector),
            NULL, &mvp);
    }

    /* ── Nacelle pylons (diagonal struts from engineering to nacelles) ── */
    {
        float pw = 0.06f;  /* pylon thickness */
        /* Left pylon */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-0.25f, 0.0f,  -0.4f, 0,0, pylon_col),
            sr_vert_c(-0.25f, 0.0f,  -0.7f, 0,1, pylon_col),
            sr_vert_c(-0.85f, 0.35f, -0.7f, 1,1, pylon_col),
            sr_vert_c(-0.85f, 0.35f, -0.4f, 1,0, pylon_col),
            NULL, &mvp);
        /* Pylon thickness (top face) */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-0.25f,      pw, -0.4f, 0,0, pylon_col),
            sr_vert_c(-0.85f, 0.35f+pw, -0.4f, 0,1, pylon_col),
            sr_vert_c(-0.85f, 0.35f+pw, -0.7f, 1,1, pylon_col),
            sr_vert_c(-0.25f,      pw, -0.7f, 1,0, pylon_col),
            NULL, &mvp);

        /* Right pylon (mirror) */
        sr_draw_quad(fb_ptr,
            sr_vert_c(0.25f, 0.0f,  -0.7f, 0,0, pylon_col),
            sr_vert_c(0.25f, 0.0f,  -0.4f, 0,1, pylon_col),
            sr_vert_c(0.85f, 0.35f, -0.4f, 1,1, pylon_col),
            sr_vert_c(0.85f, 0.35f, -0.7f, 1,0, pylon_col),
            NULL, &mvp);
        sr_draw_quad(fb_ptr,
            sr_vert_c(0.25f,      pw, -0.7f, 0,0, pylon_col),
            sr_vert_c(0.85f, 0.35f+pw, -0.7f, 0,1, pylon_col),
            sr_vert_c(0.85f, 0.35f+pw, -0.4f, 1,1, pylon_col),
            sr_vert_c(0.25f,      pw, -0.4f, 1,0, pylon_col),
            NULL, &mvp);
    }

    /* ── Nacelles (elongated boxes, raised on pylons) ── */
    {
        float nw = 0.12f;  /* nacelle half-width */
        float nh = 0.1f;   /* nacelle half-height */
        float ny = 0.4f;   /* nacelle Y center */
        float nz0 = -1.3f; /* aft */
        float nz1 = 0.0f;  /* fore */
        float nx = 0.85f;  /* X offset from center */

        /* Left nacelle */
        sfa_draw_box(fb_ptr, &mvp,
                     -nx - nw, ny - nh, nz0,
                     -nx + nw, ny + nh, nz1,
                     nacelle_t, nacelle_s, nacelle_b);

        /* Right nacelle */
        sfa_draw_box(fb_ptr, &mvp,
                      nx - nw, ny - nh, nz0,
                      nx + nw, ny + nh, nz1,
                     nacelle_t, nacelle_s, nacelle_b);

        /* Bussard collectors (front caps — red glow) */
        uint32_t bussard = 0xFF2222FF;  /* red in ABGR */
        sr_draw_quad(fb_ptr,
            sr_vert_c(-nx-nw, ny-nh, nz1+0.01f, 0,0, bussard),
            sr_vert_c(-nx-nw, ny+nh, nz1+0.01f, 0,1, bussard),
            sr_vert_c(-nx+nw, ny+nh, nz1+0.01f, 1,1, bussard),
            sr_vert_c(-nx+nw, ny-nh, nz1+0.01f, 1,0, bussard),
            NULL, &mvp);
        sr_draw_quad(fb_ptr,
            sr_vert_c(nx-nw, ny-nh, nz1+0.01f, 0,0, bussard),
            sr_vert_c(nx-nw, ny+nh, nz1+0.01f, 0,1, bussard),
            sr_vert_c(nx+nw, ny+nh, nz1+0.01f, 1,1, bussard),
            sr_vert_c(nx+nw, ny-nh, nz1+0.01f, 1,0, bussard),
            NULL, &mvp);
    }

    /* ── Engine glow (exhaust from nacelle rears) ── */
    if (s->current_speed > 0.1f) {
        float speed_frac = s->current_speed / sfa_speed_values[SFA_NUM_SPEEDS - 1];
        float glow_len = 0.4f + 0.9f * speed_frac;
        float pulse = 0.7f + 0.3f * sinf(sfa.time * 12.0f);
        uint8_t gr = (uint8_t)(255.0f * pulse);
        uint8_t gg = (uint8_t)(100.0f * pulse);
        uint32_t glow_col = 0xFF000000 | (uint32_t)(0x22) << 16 | (uint32_t)gg << 8 | gr;

        float nx = 0.85f;
        float ny = 0.4f;
        float nz0 = -1.3f;

        /* Left nacelle exhaust */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(-nx, ny + 0.05f, nz0,             0.5f, 0, glow_col),
            sr_vert_c(-nx - 0.06f, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            sr_vert_c(-nx + 0.06f, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            NULL, &mvp);
        sr_draw_triangle(fb_ptr,
            sr_vert_c(-nx, ny - 0.05f, nz0,             0.5f, 0, glow_col),
            sr_vert_c(-nx + 0.06f, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            sr_vert_c(-nx - 0.06f, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            NULL, &mvp);

        /* Right nacelle exhaust */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(nx, ny + 0.05f, nz0,             0.5f, 0, glow_col),
            sr_vert_c(nx + 0.06f, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            sr_vert_c(nx - 0.06f, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            NULL, &mvp);
        sr_draw_triangle(fb_ptr,
            sr_vert_c(nx, ny - 0.05f, nz0,             0.5f, 0, glow_col),
            sr_vert_c(nx - 0.06f, ny, nz0 - glow_len,  0, 1, 0xFF000000),
            sr_vert_c(nx + 0.06f, ny, nz0 - glow_len,  1, 1, 0xFF000000),
            NULL, &mvp);

        /* Main impulse engine exhaust (rear of engineering hull) */
        sr_draw_triangle(fb_ptr,
            sr_vert_c(0, 0.05f, -1.4f,              0.5f, 0, glow_col),
            sr_vert_c(-0.12f, 0, -1.4f - glow_len*0.6f, 0, 1, 0xFF000000),
            sr_vert_c( 0.12f, 0, -1.4f - glow_len*0.6f, 1, 1, 0xFF000000),
            NULL, &mvp);
    }
}

/* ── 3D Starfield (pixel-based with palette shading) ─────────────── */

static void sfa_draw_starfield(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                 float cam_x, float cam_z) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    float spacing = SFA_GRID_SPACING;
    float view_range = 50.0f;
    float y_range = 30.0f;
    float y_center = 5.0f;

    float x_start = floorf((cam_x - view_range) / spacing) * spacing;
    float z_start = floorf((cam_z - view_range) / spacing) * spacing;
    float y_start = floorf((y_center - y_range) / spacing) * spacing;

    /* Palette base indices for star tints */
    static const int star_tints[4] = { 8, 47, 18, 12 };
    /* white(C7DCD0), blue(4D9BE6), yellow(F9C22B), red(EA4F36) */

    for (float gx = x_start; gx < cam_x + view_range; gx += spacing) {
        for (float gz = z_start; gz < cam_z + view_range; gz += spacing) {
            for (float gy = y_start; gy < y_center + y_range; gy += spacing) {
                int ix = (int)floorf(gx / spacing);
                int iy = (int)floorf(gy / spacing);
                int iz = (int)floorf(gz / spacing);
                uint32_t seed = (uint32_t)(ix * 73856093u ^ iy * 83492791u ^ iz * 19349663u);

                /* Skip ~50% of cells (with doubled spacing = ~25% total density) */
                if ((seed & 3) == 0) continue;

                /* 1 star per cell */
                int n_stars = 1;
                for (int si = 0; si < n_stars; si++) {
                    seed = seed * 1103515245u + 12345u + (uint32_t)si * 7u;
                    float ox = (float)((seed >> 4) & 0xFF) / 255.0f * spacing;
                    seed = seed * 1103515245u + 12345u;
                    float oy = (float)((seed >> 4) & 0xFF) / 255.0f * spacing;
                    seed = seed * 1103515245u + 12345u;
                    float oz = (float)((seed >> 4) & 0xFF) / 255.0f * spacing;
                    seed = seed * 1103515245u + 12345u;
                    float brightness = 0.12f + (float)((seed >> 4) & 0xFF) / 255.0f * 0.6f;

                    float star_x = gx + ox;
                    float star_y = gy + oy;
                    float star_z = gz + oz;

                    /* Distance fade */
                    float ddx = star_x - cam_x;
                    float ddz = star_z - cam_z;
                    float dist2 = ddx * ddx + ddz * ddz;
                    if (dist2 > view_range * view_range) continue;
                    float dist_fade = 1.0f - dist2 / (view_range * view_range);
                    brightness *= dist_fade;
                    if (brightness < 0.02f) continue;

                    /* Project to screen */
                    sr_vec4 clip = sr_mat4_mul_v4(*vp, sr_v4(star_x, star_y, star_z, 1.0f));
                    if (clip.w < 0.1f) continue; /* behind camera */
                    float inv_w = 1.0f / clip.w;
                    float ndc_x = clip.x * inv_w;
                    float ndc_y = clip.y * inv_w;

                    int sx = (int)((ndc_x * 0.5f + 0.5f) * W);
                    int sy = (int)((1.0f - (ndc_y * 0.5f + 0.5f)) * H);

                    if (sx < 0 || sx >= W || sy < 0 || sy >= H) continue;

                    /* Tint and shade */
                    seed = seed * 1103515245u + 12345u;
                    int tint = (seed >> 8) % 4;
                    int base_col = star_tints[tint];
                    /* Map brightness [0,1] → shade [2, PAL_MID_ROW+2] */
                    int shade = 2 + (int)(brightness * (float)(PAL_MID_ROW));
                    uint32_t col = sfa_pal_shade(base_col, shade);

                    /* Close stars (< 40% range) = 2x2, far = 1x1 */
                    float dist_norm = dist2 / (view_range * view_range);
                    if (dist_norm < 0.16f && brightness > 0.25f) {
                        /* 2x2 pixel star */
                        px[sy * W + sx] = col;
                        if (sx + 1 < W) px[sy * W + sx + 1] = col;
                        if (sy + 1 < H) px[(sy + 1) * W + sx] = col;
                        if (sx + 1 < W && sy + 1 < H) px[(sy + 1) * W + sx + 1] = col;
                    } else {
                        /* 1x1 pixel star */
                        px[sy * W + sx] = col;
                    }
                }
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

/* ── Target drawing & projection ─────────────────────────────────── */

/* Draw Klingon Bird of Prey — swept wings, central command pod, neck */
static void sfa_draw_target_ship(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                   float tx, float tz) {
    uint32_t hull_t = sfa_pal_abgr(30); /* 239063 dark green */
    uint32_t hull_s = sfa_pal_abgr(29); /* 165a4c darker green */
    uint32_t hull_b = sfa_pal_abgr(35); /* 374e4a darkest */
    uint32_t wing_t = sfa_pal_abgr(36); /* 547e64 olive green */
    uint32_t wing_s = sfa_pal_abgr(35); /* 374e4a */
    uint32_t wing_b = sfa_pal_abgr(34); /* 313638 near-black */
    uint32_t head_t = sfa_pal_abgr(31); /* 1ebc73 bright green */
    uint32_t head_s = sfa_pal_abgr(30);
    uint32_t gun_col = sfa_pal_abgr(15); /* e83b3b red — disruptor */

    sr_mat4 model = sr_mat4_translate(tx, 0.0f, tz);
    sr_mat4 mvp = sr_mat4_mul(*vp, model);

    /* Central body (aft section) */
    sfa_draw_box(fb_ptr, &mvp,
                 -0.3f, -0.1f, -0.9f,
                  0.3f,  0.15f, 0.0f,
                 hull_t, hull_s, hull_b);

    /* Neck (connecting body to head) */
    sfa_draw_box(fb_ptr, &mvp,
                 -0.1f, -0.05f, 0.0f,
                  0.1f,  0.08f, 0.7f,
                 hull_t, hull_s, hull_b);

    /* Command pod (head) */
    sfa_draw_box(fb_ptr, &mvp,
                 -0.2f, -0.08f, 0.7f,
                  0.2f,  0.12f, 1.1f,
                 head_t, head_s, hull_b);

    /* Disruptor cannon (front of head) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.08f, -0.02f, 1.11f, 0,0, gun_col),
        sr_vert_c(-0.08f,  0.06f, 1.11f, 0,1, gun_col),
        sr_vert_c( 0.08f,  0.06f, 1.11f, 1,1, gun_col),
        sr_vert_c( 0.08f, -0.02f, 1.11f, 1,0, gun_col),
        NULL, &mvp);

    /* Left swept wing — angled down and forward */
    /* Wing root at body, tip swept forward and outward */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.3f,  0.05f, -0.6f, 0,0, wing_t),
        sr_vert_c(-0.3f,  0.05f, -0.1f, 0,1, wing_t),
        sr_vert_c(-1.4f, -0.15f,  0.4f, 1,1, wing_t),
        sr_vert_c(-1.4f, -0.15f, -0.2f, 1,0, wing_t),
        NULL, &mvp);
    /* Wing bottom */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.3f, -0.02f, -0.6f, 0,0, wing_b),
        sr_vert_c(-1.4f, -0.18f, -0.2f, 1,0, wing_b),
        sr_vert_c(-1.4f, -0.18f,  0.4f, 1,1, wing_b),
        sr_vert_c(-0.3f, -0.02f, -0.1f, 0,1, wing_b),
        NULL, &mvp);
    /* Wing leading edge */
    sr_draw_quad(fb_ptr,
        sr_vert_c(-0.3f, -0.02f, -0.1f, 0,0, wing_s),
        sr_vert_c(-1.4f, -0.18f,  0.4f, 1,0, wing_s),
        sr_vert_c(-1.4f, -0.15f,  0.4f, 1,1, wing_s),
        sr_vert_c(-0.3f,  0.05f, -0.1f, 0,1, wing_s),
        NULL, &mvp);
    /* Wingtip disruptor */
    sfa_draw_box(fb_ptr, &mvp,
                 -1.5f, -0.18f, 0.1f,
                 -1.35f,-0.1f,  0.5f,
                 gun_col, gun_col, gun_col);

    /* Right swept wing (mirror) */
    sr_draw_quad(fb_ptr,
        sr_vert_c(0.3f,  0.05f, -0.1f, 0,0, wing_t),
        sr_vert_c(0.3f,  0.05f, -0.6f, 0,1, wing_t),
        sr_vert_c(1.4f, -0.15f, -0.2f, 1,1, wing_t),
        sr_vert_c(1.4f, -0.15f,  0.4f, 1,0, wing_t),
        NULL, &mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(0.3f, -0.02f, -0.1f, 0,0, wing_b),
        sr_vert_c(0.3f, -0.02f, -0.6f, 0,1, wing_b),
        sr_vert_c(1.4f, -0.18f, -0.2f, 1,1, wing_b),
        sr_vert_c(1.4f, -0.18f,  0.4f, 1,0, wing_b),
        NULL, &mvp);
    sr_draw_quad(fb_ptr,
        sr_vert_c(0.3f,  0.05f, -0.1f, 0,0, wing_s),
        sr_vert_c(0.3f, -0.02f, -0.1f, 0,1, wing_s),
        sr_vert_c(1.4f, -0.18f,  0.4f, 1,1, wing_s),
        sr_vert_c(1.4f, -0.15f,  0.4f, 1,0, wing_s),
        NULL, &mvp);
    sfa_draw_box(fb_ptr, &mvp,
                 1.35f, -0.18f, 0.1f,
                 1.5f,  -0.1f,  0.5f,
                 gun_col, gun_col, gun_col);
}

/* Project a world point to framebuffer coords. Returns false if behind camera. */
static bool sfa_project_to_screen(const sr_mat4 *vp, float wx, float wy, float wz,
                                    int W, int H, int *out_sx, int *out_sy, float *out_w) {
    sr_vec4 clip = sr_mat4_mul_v4(*vp, sr_v4(wx, wy, wz, 1.0f));
    *out_w = clip.w;
    if (clip.w < 0.1f) {
        /* Behind camera — compute direction for edge indicator */
        *out_sx = (clip.x < 0) ? W - 1 : 0;
        *out_sy = (clip.y > 0) ? 0 : H - 1;
        return false;
    }
    float inv_w = 1.0f / clip.w;
    float ndc_x = clip.x * inv_w;
    float ndc_y = clip.y * inv_w;
    *out_sx = (int)((ndc_x * 0.5f + 0.5f) * W);
    *out_sy = (int)((1.0f - (ndc_y * 0.5f + 0.5f)) * H);
    return true;
}

/* Draw targeting reticle brackets around a screen point */
static void sfa_draw_reticle(uint32_t *px, int W, int H,
                               int cx, int cy, bool selected) {
    uint32_t col = selected ? sfa_pal_abgr(15) : sfa_pal_abgr(12); /* red / orange */
    int r = selected ? 10 : 8;  /* bracket size */
    int t = 3;                  /* bracket arm length */

    /* Four corner brackets */
    for (int i = 0; i < t; i++) {
        /* Top-left */
        int x, y;
        x = cx - r + i; y = cy - r;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        x = cx - r; y = cy - r + i;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        /* Top-right */
        x = cx + r - i; y = cy - r;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        x = cx + r; y = cy - r + i;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        /* Bottom-left */
        x = cx - r + i; y = cy + r;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        x = cx - r; y = cy + r - i;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        /* Bottom-right */
        x = cx + r - i; y = cy + r;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
        x = cx + r; y = cy + r - i;
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = col;
    }
}

/* Draw off-screen target indicator clamped to screen edge */
static void sfa_draw_offscreen_indicator(uint32_t *px, int W, int H,
                                           int tx, int ty, bool selected) {
    /* Clamp to screen edge with margin */
    int margin = 12;
    int cx = tx, cy = ty;
    if (cx < margin) cx = margin;
    if (cx >= W - margin) cx = W - margin - 1;
    if (cy < margin + 14) cy = margin + 14; /* avoid HUD top bar */
    if (cy >= H - margin) cy = H - margin - 1;

    uint32_t col = selected ? sfa_pal_abgr(15) : sfa_pal_abgr(12);

    /* Draw small arrow pointing toward target */
    float dx = (float)(tx - cx);
    float dy = (float)(ty - cy);
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) len = 1.0f;
    dx /= len; dy /= len;

    /* Arrow tip */
    int ax = cx + (int)(dx * 5);
    int ay = cy + (int)(dy * 5);
    /* Draw a 3x3 filled square at the indicator position */
    for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
            int px2 = cx + ox, py2 = cy + oy;
            if (px2 >= 0 && px2 < W && py2 >= 0 && py2 < H)
                px[py2 * W + px2] = col;
        }
    /* Arrow line */
    if (ax >= 0 && ax < W && ay >= 0 && ay < H)
        px[ay * W + ax] = col;
    int mx = cx + (int)(dx * 3);
    int my = cy + (int)(dy * 3);
    if (mx >= 0 && mx < W && my >= 0 && my < H)
        px[my * W + mx] = col;
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
    float best_dist = 25.0f;  /* pixel threshold for hover */

    for (int i = 0; i < sfa.npc_count; i++) {
        sfa_ship *npc = &sfa.npcs[i];
        int scr_x, scr_y;
        float scr_w;
        if (!sfa_project_to_screen(vp, npc->x, 0.2f, npc->z, fb_w, fb_h, &scr_x, &scr_y, &scr_w))
            continue;
        float ddx = sfa.mouse_fb_x - (float)scr_x;
        float ddy = sfa.mouse_fb_y - (float)scr_y;
        float dist = sqrtf(ddx*ddx + ddy*ddy);
        if (dist < best_dist) {
            best_dist = dist;
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

    /* Check target click (on-screen reticle or off-screen indicator) */
    {
        int tdx = (int)fx - sfa.target_screen_x;
        int tdy = (int)fy - sfa.target_screen_y;
        if (tdx * tdx + tdy * tdy < 20 * 20) {
            sfa.target_selected = !sfa.target_selected;
            return true;
        }
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

/* ── Mouse input for targeting ────────────────────────────────────── */

static void sfa_handle_mouse_move(float sx, float sy) {
    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);
    sfa.mouse_fb_x = fx;
    sfa.mouse_fb_y = fy;
}

static void sfa_handle_mouse_click(float sx, float sy) {
    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);
    sfa.mouse_fb_x = fx;
    sfa.mouse_fb_y = fy;

    /* If hovering an NPC, select/deselect it */
    if (sfa.hovered_npc >= 0) {
        if (sfa.selected_npc == sfa.hovered_npc)
            sfa.selected_npc = -1;  /* deselect */
        else
            sfa.selected_npc = sfa.hovered_npc;
    } else {
        sfa.selected_npc = -1;  /* click empty space to deselect */
    }
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
        case SAPP_KEYCODE_TAB:
            /* Cycle through targets */
            if (sfa.npc_count > 0) {
                sfa.selected_npc++;
                if (sfa.selected_npc >= sfa.npc_count)
                    sfa.selected_npc = -1;  /* deselect after last */
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

/* ── Main scene draw ─────────────────────────────────────────────── */

static void draw_space_fleet_scene(sr_framebuffer *fb_ptr, float dt) {
    if (!sfa.initialized) sfa_init();

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

    /* Update hover detection */
    sfa_update_hover(&vp, W, H);

    /* Draw world */
    sfa_draw_starfield(fb_ptr, &vp, s->x, s->z);
    sfa_draw_arena_boundary(fb_ptr, &vp);

    /* Draw target (Klingon ship) */
    sfa_draw_target_ship(fb_ptr, &vp, sfa.target_x, sfa.target_z);

    /* Project target to screen for reticle/indicator */
    {
        int tsx, tsy;
        float tw;
        bool on_screen = sfa_project_to_screen(&vp, sfa.target_x, 0.0f, sfa.target_z,
                                                 W, H, &tsx, &tsy, &tw);
        sfa.target_screen_x = tsx;
        sfa.target_screen_y = tsy;
        sfa.target_on_screen = on_screen && tsx >= 0 && tsx < W && tsy >= 0 && tsy < H;

        if (sfa.target_on_screen) {
            sfa_draw_reticle(px, W, H, tsx, tsy, sfa.target_selected);
        } else {
            /* Clamp to screen edge */
            int cx = tsx, cy = tsy;
            if (!on_screen) {
                /* Behind camera — put indicator at bottom edge toward target */
                float dx = sfa.target_x - s->x;
                float dz = sfa.target_z - s->z;
                float a = atan2f(-dx, dz) - s->visual_heading;
                cx = W / 2 + (int)(sinf(a) * W * 0.4f);
                cy = (cosf(a) < 0) ? H - 12 : 12;
            }
            sfa_draw_offscreen_indicator(px, W, H, cx, cy, sfa.target_selected);
            sfa.target_screen_x = cx;
            sfa.target_screen_y = cy;
        }
    }

    /* Draw player ship */
    sfa_draw_ship(fb_ptr, &vp, s);

    /* Draw NPC ships */
    for (int i = 0; i < sfa.npc_count; i++)
        sfa_draw_ship(fb_ptr, &vp, &sfa.npcs[i]);

    /* Draw targeting brackets on NPC ships */
    {
        for (int i = 0; i < sfa.npc_count; i++) {
            sfa_ship *npc = &sfa.npcs[i];
            int scr_x, scr_y;
            float scr_w;
            if (!sfa_project_to_screen(&vp, npc->x, 0.2f, npc->z, W, H, &scr_x, &scr_y, &scr_w))
                continue;

            /* Bracket size based on distance */
            float ddx = npc->x - s->x;
            float ddz = npc->z - s->z;
            float dist = sqrtf(ddx*ddx + ddz*ddz);
            int bracket_half = (int)(600.0f / (dist + 5.0f));
            if (bracket_half < 8) bracket_half = 8;
            if (bracket_half > 40) bracket_half = 40;

            uint32_t bracket_col;
            if (i == sfa.selected_npc) {
                /* Selected: bright blue glow with pulse */
                float pulse = 0.7f + 0.3f * sinf(sfa.time * 6.0f);
                uint8_t b = (uint8_t)(255.0f * pulse);
                uint8_t g = (uint8_t)(220.0f * pulse);
                bracket_col = 0xFF000000 | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)(100);
                /* Draw double brackets for selected */
                sfa_draw_targeting_brackets(px, W, H, scr_x, scr_y, bracket_half + 2, bracket_col);
                sfa_draw_targeting_brackets(px, W, H, scr_x, scr_y, bracket_half, bracket_col);

                /* Target info text */
                char tbuf[32];
                snprintf(tbuf, sizeof(tbuf), "TGT: %.0fm", dist);
                sr_draw_text_shadow(px, W, H, scr_x - 18, scr_y + bracket_half + 4,
                                     tbuf, SFA_TARGET_SELECTED, SFA_HUD_SHADOW);
            } else if (i == sfa.hovered_npc) {
                /* Hovered: highlighted brackets */
                bracket_col = 0xFFFFEE88;  /* bright warm white */
                sfa_draw_targeting_brackets(px, W, H, scr_x, scr_y, bracket_half, bracket_col);

                /* Hover hint */
                sr_draw_text_shadow(px, W, H, scr_x - 12, scr_y + bracket_half + 4,
                                     "CLICK", 0xFFCCCCCC, SFA_HUD_SHADOW);
            } else {
                /* Default: dim brackets */
                bracket_col = 0xFF555555;
                sfa_draw_targeting_brackets(px, W, H, scr_x, scr_y, bracket_half, bracket_col);
            }
        }
    }

    /* Draw HUD */
    sfa_draw_hud(fb_ptr, s);
}

#endif /* SR_SCENE_SPACE_FLEET_H */
