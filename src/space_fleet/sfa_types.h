/*  sfa_types.h — Constants, structs, and global state for Space Fleet Assault.
 *  Header-only. Depends on sr_app.h (for pal_colors, pal_shift_lut, etc). */
#ifndef SFA_TYPES_H
#define SFA_TYPES_H

#include <math.h>
#include <stdbool.h>

/* ── Constants ───────────────────────────────────────────────────── */

#define SFA_PI           3.14159265f
#define SFA_TWO_PI       6.28318530f
#define SFA_DEG2RAD      (SFA_PI / 180.0f)

#define SFA_ARENA_SIZE   160.0f     /* half-extent of playable area */
#define SFA_GRID_SPACING 10.0f      /* starfield grid spacing (sparser) */

/* Speed levels (impulse) */
#define SFA_SPEED_STOP   0
#define SFA_SPEED_QUARTER 1
#define SFA_SPEED_HALF   2
#define SFA_SPEED_3QUARTER 3
#define SFA_SPEED_FULL   4
#define SFA_NUM_SPEEDS   5

static const float sfa_speed_values[SFA_NUM_SPEEDS] = {
    0.0f, 1.5f, 3.0f, 4.5f, 6.0f
};
static const char *sfa_speed_names[SFA_NUM_SPEEDS] = {
    "ALL STOP", "1/4 IMPULSE", "1/2 IMPULSE", "3/4 IMPULSE", "FULL IMPULSE"
};

/* Ship turn rate (radians/sec) */
#define SFA_TURN_RATE    2.0f
#define SFA_STEER_DISC_R 6.0f    /* world-space steering disc radius */

/* Sensor ranges */
#define SFA_SENSOR_SHORT  20.0f     /* short range sensor radius */
#define SFA_SENSOR_LONG   60.0f    /* long range sensor radius */

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

/* Weapons */
#define SFA_PHASER_RANGE     40.0f
#define SFA_PHASER_DAMAGE    8.0f
#define SFA_PHASER_COOLDOWN  2.0f
#define SFA_PHASER_BEAM_TIME 0.4f
#define SFA_PHASER_ARC       2.618f   /* ~150 deg half-arc = broadside */

#define SFA_TORP_RANGE       55.0f
#define SFA_TORP_DAMAGE      25.0f
#define SFA_TORP_COOLDOWN    5.0f
#define SFA_TORP_SPEED       20.0f
#define SFA_TORP_ARC         0.524f   /* ~30 deg half-arc = forward only */
#define SFA_TORP_LIFETIME    4.0f
#define SFA_TORP_HIT_RADIUS  1.5f

#define SFA_MAX_BEAMS        8
#define SFA_MAX_TORPS        16
#define SFA_MAX_EXPLOSIONS   16

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
    int      ship_class;      /* SHIP_CLASS_* enum — determines 3D model */

    /* Weapons */
    float phaser_cooldown;    /* seconds until phaser ready (0 = ready) */
    float torpedo_cooldown;   /* seconds until torpedo ready (0 = ready) */
    int   torpedoes_remaining;
    bool  alive;

    /* NPC AI: backoff timer — retreat after close engagement */
    float backoff_timer;      /* >0 = retreating, counts down */
} sfa_ship;

/* ── Combat visual effects ──────────────────────────────────────── */

typedef struct {
    float x0, z0;        /* origin (firer position) */
    float x1, z1;        /* target position */
    float timer;
    uint32_t color;
    bool active;
    int source;          /* -1 = player, 0+ = NPC index */
    int target;          /* -1 = player, 0+ = NPC index */
} sfa_beam;

typedef struct {
    float x, z;
    float dx, dz;        /* velocity */
    float timer;
    int   owner;         /* -1 = player, 0..N = npc index */
    int   target;        /* npc index, or -1 = targets player */
    uint32_t color;
    bool  active;
} sfa_torpedo;

typedef struct {
    float x, z;
    float timer;
    float max_timer;
    uint32_t color;
    bool active;
} sfa_explosion;

/* Game phases */
enum {
    SFA_PHASE_BRIEFING,     /* pre-mission briefing screen */
    SFA_PHASE_COMBAT,       /* active gameplay */
    SFA_PHASE_VICTORY,      /* "MISSION COMPLETE" overlay, 5-second timer */
    SFA_PHASE_STATS,        /* post-mission stats summary */
};

/* Mission stats (tracked during combat) */
typedef struct {
    int   phasers_fired;
    int   torpedoes_fired;
    int   enemies_destroyed;
    float damage_dealt;
    float damage_taken;
    float combat_time;       /* seconds in combat phase */
} sfa_mission_stats;

typedef struct {
    sfa_ship player;
    sfa_ship npcs[SFA_MAX_NPC];
    int      npc_count;
    float    time;
    bool     initialized;

    /* Game phase */
    int      phase;
    float    phase_timer;        /* countdown for phase transitions */
    sfa_mission_stats stats;

    /* Targeting */
    int      hovered_npc;         /* NPC index under cursor, -1 = none */
    int      selected_npc;        /* NPC index locked on, -1 = none */
    float    mouse_fb_x, mouse_fb_y;  /* mouse pos in fb coords */
    float    cam_target_yaw;      /* smoothed camera yaw toward target */
    sr_mat4  last_vp;             /* cached VP matrix for input-time hover */
    sr_mat4  last_inv_vp;         /* inverse VP for unprojection */
    sr_vec3  last_eye;            /* camera eye position */
    int      last_fb_w, last_fb_h;

    /* Weapon hover (for arc display) */
    int      hovered_weapon;      /* -1=none, 0=phaser, 1=torpedo */

    /* Touch controls */
    bool     touch_steering;      /* is user dragging to steer? */
    float    touch_steer_cx;      /* center of steering circle (fb coords) */
    float    touch_steer_cy;
    float    touch_steer_angle;   /* current angle from touch */
    bool     touch_throttle;      /* throttle touch active */
    bool     touch_turn_left;     /* mobile turn-left button held */
    bool     touch_turn_right;    /* mobile turn-right button held */

    /* Sensors */
    bool     long_range_sensors;  /* false = short range (60), true = long range (160) */

    /* Combat effects */
    sfa_beam       beams[SFA_MAX_BEAMS];
    sfa_torpedo    torpedoes[SFA_MAX_TORPS];
    sfa_explosion  explosions[SFA_MAX_EXPLOSIONS];
} sfa_state;

static sfa_state sfa;

/* ── Key held state (for smooth turning) ─────────────────────────── */

static bool sfa_key_left  = false;
static bool sfa_key_right = false;
static bool sfa_key_up    = false;
static bool sfa_key_down  = false;

#endif /* SFA_TYPES_H */
