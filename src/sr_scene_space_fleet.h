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
    0.0f, 3.0f, 6.0f, 9.0f, 12.0f
};
static const char *sfa_speed_names[SFA_NUM_SPEEDS] = {
    "ALL STOP", "1/4 IMPULSE", "1/2 IMPULSE", "3/4 IMPULSE", "FULL IMPULSE"
};

/* Ship turn rate (radians/sec) */
#define SFA_TURN_RATE    2.0f
#define SFA_STEER_DISC_R 4.0f    /* world-space steering disc radius */

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
    s->phaser_cooldown = 0;
    s->torpedo_cooldown = 0;
    s->torpedoes_remaining = 20;
    s->alive = true;
    s->current_speed = 0;
    s->backoff_timer = 0;
    for (int i = 0; i < 6; i++)
        s->shields[i] = 100.0f;
}

static void sfa_init(void) {
    memset(&sfa, 0, sizeof(sfa));

    /* Player ship — default orange */
    sfa_init_ship(&sfa.player, 0.0f, 0.0f, 0.0f, 0, 0);

    /* NPC Klingon ships */
    sfa.npc_count = 2;
    sfa_init_ship(&sfa.npcs[0], 15.0f, 20.0f, SFA_PI * 0.75f, 0, 0);
    sfa.npcs[0].speed_level = SFA_SPEED_QUARTER;
    sfa_init_ship(&sfa.npcs[1], 30.0f, 20.0f, SFA_PI * 0.25f, 0, 0);
    sfa.npcs[1].speed_level = SFA_SPEED_HALF;

    sfa.hovered_npc = -1;
    sfa.selected_npc = -1;
    sfa.cam_target_yaw = 0.0f;

    sfa.phase = SFA_PHASE_BRIEFING;
    sfa.phase_timer = 0;
    memset(&sfa.stats, 0, sizeof(sfa.stats));

    sfa.initialized = true;
}

/* ── Normalize angle to [-PI, PI] ────────────────────────────────── */

static float sfa_normalize_angle(float a) {
    while (a >  SFA_PI) a -= SFA_TWO_PI;
    while (a < -SFA_PI) a += SFA_TWO_PI;
    return a;
}

/* ── Combat helpers ─────────────────────────────────────────────── */

/* Which shield facing (0-5) is hit by an attack from (ax,az) on ship s? */
static int sfa_shield_facing(sfa_ship *s, float ax, float az) {
    float dx = ax - s->x;
    float dz = az - s->z;
    float bearing = sfa_normalize_angle(atan2f(dx, dz) - s->heading);
    float deg = bearing * 180.0f / SFA_PI;
    /* F(-30..30), FR(30..90), AR(90..150), A(>150 or <-150), AL(-150..-90), FL(-90..-30) */
    if (deg >= -30.0f && deg < 30.0f)   return 0; /* F */
    if (deg >= 30.0f  && deg < 90.0f)   return 1; /* FR */
    if (deg >= 90.0f  && deg < 150.0f)  return 2; /* AR */
    if (deg >= 150.0f || deg < -150.0f) return 3; /* A */
    if (deg >= -150.0f && deg < -90.0f) return 4; /* AL */
    return 5; /* FL */
}

static void sfa_apply_damage(sfa_ship *target, float damage, int shield_idx) {
    float remaining = damage;
    if (target->shields[shield_idx] > 0) {
        if (target->shields[shield_idx] >= remaining) {
            target->shields[shield_idx] -= remaining;
            remaining = 0;
        } else {
            remaining -= target->shields[shield_idx];
            target->shields[shield_idx] = 0;
        }
    }
    if (remaining > 0) {
        target->hull -= remaining;
        if (target->hull <= 0) {
            target->hull = 0;
            target->alive = false;
        }
    }
}

/* Is target at (tx,tz) within weapon arc (half-angle) from ship s? */
static bool sfa_in_weapon_arc(sfa_ship *s, float tx, float tz, float arc_half) {
    float dx = tx - s->x;
    float dz = tz - s->z;
    float bearing = atan2f(-dx, dz);  /* matches heading convention */
    float diff = sfa_normalize_angle(bearing - s->heading);
    return (diff >= -arc_half && diff <= arc_half);
}

static void sfa_spawn_beam(float x0, float z0, float x1, float z1,
                            uint32_t color, int source, int target) {
    for (int i = 0; i < SFA_MAX_BEAMS; i++) {
        if (!sfa.beams[i].active) {
            sfa.beams[i] = (sfa_beam){
                x0, z0, x1, z1, SFA_PHASER_BEAM_TIME, color, true, source, target
            };
            return;
        }
    }
}

static void sfa_spawn_explosion(float x, float z, float duration, uint32_t color) {
    for (int i = 0; i < SFA_MAX_EXPLOSIONS; i++) {
        if (!sfa.explosions[i].active) {
            sfa.explosions[i] = (sfa_explosion){ x, z, duration, duration, color, true };
            return;
        }
    }
}

static void sfa_spawn_torpedo_proj(sfa_ship *firer, int owner_idx, int target_idx,
                                    float tx, float tz, uint32_t color) {
    for (int i = 0; i < SFA_MAX_TORPS; i++) {
        if (!sfa.torpedoes[i].active) {
            float dx = tx - firer->x;
            float dz = tz - firer->z;
            float len = sqrtf(dx * dx + dz * dz);
            if (len < 0.01f) return;
            sfa.torpedoes[i] = (sfa_torpedo){
                firer->x, firer->z,
                (dx / len) * SFA_TORP_SPEED, (dz / len) * SFA_TORP_SPEED,
                SFA_TORP_LIFETIME, owner_idx, target_idx, color, true
            };
            return;
        }
    }
}

/* Fire phasers — instant hit beam weapon.
   source_idx/target_idx: -1 = player, 0+ = NPC index */
static void sfa_fire_phaser(sfa_ship *firer, sfa_ship *target,
                              int source_idx, int target_idx) {
    if (firer->phaser_cooldown > 0) return;
    if (!target->alive) return;

    float dx = target->x - firer->x;
    float dz = target->z - firer->z;
    float dist = sqrtf(dx * dx + dz * dz);
    if (dist > SFA_PHASER_RANGE) return;
    if (!sfa_in_weapon_arc(firer, target->x, target->z, SFA_PHASER_ARC)) return;

    firer->phaser_cooldown = SFA_PHASER_COOLDOWN;

    int shield_idx = sfa_shield_facing(target, firer->x, firer->z);
    float pre_hull = target->hull;
    sfa_apply_damage(target, SFA_PHASER_DAMAGE, shield_idx);

    /* Track stats for player */
    if (source_idx == -1) {
        sfa.stats.phasers_fired++;
        sfa.stats.damage_dealt += SFA_PHASER_DAMAGE;
        if (!target->alive && pre_hull > 0)
            sfa.stats.enemies_destroyed++;
    } else {
        sfa.stats.damage_taken += SFA_PHASER_DAMAGE;
    }

    sfa_spawn_beam(firer->x, firer->z, target->x, target->z,
                    0xFF4488FF, source_idx, target_idx);
    sfa_spawn_explosion(target->x, target->z, 0.3f, 0xFF55CCFF);
}

/* Fire torpedo — forward-arc projectile */
static void sfa_fire_torpedo(sfa_ship *firer, sfa_ship *target,
                              int owner_idx, int target_idx) {
    if (firer->torpedo_cooldown > 0) return;
    if (firer->torpedoes_remaining <= 0) return;
    if (!target->alive) return;
    if (!sfa_in_weapon_arc(firer, target->x, target->z, SFA_TORP_ARC)) return;

    firer->torpedo_cooldown = SFA_TORP_COOLDOWN;
    firer->torpedoes_remaining--;

    /* Track stats for player */
    if (owner_idx == -1) sfa.stats.torpedoes_fired++;

    sfa_spawn_torpedo_proj(firer, owner_idx, target_idx,
                           target->x, target->z, 0xFF3366FF); /* reddish glow */
}

/* ── Update ──────────────────────────────────────────────────────── */

static void sfa_update(float dt) {
    sfa_ship *s = &sfa.player;

    /* Apply continuous keyboard steering (positive heading = screen-right) */
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

    /* Tick player weapon cooldowns */
    s->phaser_cooldown -= dt;
    if (s->phaser_cooldown < 0) s->phaser_cooldown = 0;
    s->torpedo_cooldown -= dt;
    if (s->torpedo_cooldown < 0) s->torpedo_cooldown = 0;

    /* Update NPC ships (combat AI + physics) */
    for (int i = 0; i < sfa.npc_count; i++) {
        sfa_ship *npc = &sfa.npcs[i];
        if (!npc->alive) continue;

        /* Tick NPC cooldowns */
        npc->phaser_cooldown -= dt;
        if (npc->phaser_cooldown < 0) npc->phaser_cooldown = 0;
        npc->torpedo_cooldown -= dt;
        if (npc->torpedo_cooldown < 0) npc->torpedo_cooldown = 0;

        /* AI decision: combat or patrol */
        float pdx = sfa.player.x - npc->x;
        float pdz = sfa.player.z - npc->z;
        float pdist = sqrtf(pdx * pdx + pdz * pdz);

        /* Tick backoff timer */
        if (npc->backoff_timer > 0) npc->backoff_timer -= dt;

        /* Border avoidance — turn toward center when near edge */
        float border_margin = 15.0f;
        bool npc_at_border = false;
        if (npc->x >  SFA_ARENA_SIZE - border_margin ||
            npc->x < -SFA_ARENA_SIZE + border_margin ||
            npc->z >  SFA_ARENA_SIZE - border_margin ||
            npc->z < -SFA_ARENA_SIZE + border_margin) {
            npc->target_heading = atan2f(npc->x, -npc->z); /* face toward center (0,0) */
            npc->speed_level = SFA_SPEED_HALF;
            npc_at_border = true;
        }

        if (npc_at_border) {
            /* Already set heading toward center above */
        } else if (npc->backoff_timer > 0) {
            /* Retreat mode — turn away from player, slow retreat */
            float away_bearing = atan2f(pdx, -pdz); /* opposite of toward-player */
            npc->target_heading = away_bearing;
            npc->speed_level = SFA_SPEED_QUARTER;
        } else if (pdist < SFA_PHASER_RANGE * 1.2f && sfa.player.alive) {
            /* Combat mode — turn toward player */
            float target_bearing = atan2f(-pdx, pdz);
            npc->target_heading = target_bearing;

            /* Too close — trigger backoff (15-30 seconds) */
            if (pdist < SFA_PHASER_RANGE * 0.1f) {
                npc->backoff_timer = 15.0f + rng_float() * 15.0f;
            }

            /* Speed based on distance */
            if (pdist > SFA_PHASER_RANGE * 0.8f)
                npc->speed_level = SFA_SPEED_HALF;
            else
                npc->speed_level = SFA_SPEED_QUARTER;

            /* Fire phasers (slightly weaker, slightly slower) */
            if (npc->phaser_cooldown <= 0 && pdist <= SFA_PHASER_RANGE &&
                sfa_in_weapon_arc(npc, sfa.player.x, sfa.player.z, SFA_PHASER_ARC)) {
                int si = sfa_shield_facing(&sfa.player, npc->x, npc->z);
                sfa_apply_damage(&sfa.player, SFA_PHASER_DAMAGE * 0.7f, si);
                sfa.stats.damage_taken += SFA_PHASER_DAMAGE * 0.7f;
                npc->phaser_cooldown = SFA_PHASER_COOLDOWN * 1.2f;
                sfa_spawn_beam(npc->x, npc->z, sfa.player.x, sfa.player.z, 0xFF22CC22, i, -1);
                sfa_spawn_explosion(sfa.player.x, sfa.player.z, 0.3f, 0xFF44FF44);
            }

            /* Fire torpedoes (slower cooldown) */
            if (npc->torpedo_cooldown <= 0 && npc->torpedoes_remaining > 0 &&
                pdist <= SFA_TORP_RANGE &&
                sfa_in_weapon_arc(npc, sfa.player.x, sfa.player.z, SFA_TORP_ARC)) {
                npc->torpedo_cooldown = SFA_TORP_COOLDOWN * 1.5f;
                npc->torpedoes_remaining--;
                sfa_spawn_torpedo_proj(npc, i, -1,
                    sfa.player.x, sfa.player.z, 0xFF22AA22);
            }
        } else {
            /* Patrol mode — slow circle */
            npc->target_heading += 0.3f * dt;
        }
        npc->target_heading = sfa_normalize_angle(npc->target_heading);

        /* NPC physics (same as player) */
        float ndiff = sfa_normalize_angle(npc->target_heading - npc->heading);
        float nmax = SFA_TURN_RATE * dt;
        if (ndiff > nmax) ndiff = nmax;
        else if (ndiff < -nmax) ndiff = -nmax;
        npc->heading += ndiff;
        npc->heading = sfa_normalize_angle(npc->heading);

        float nvdiff = sfa_normalize_angle(npc->heading - npc->visual_heading);
        npc->visual_heading += nvdiff * 8.0f * dt;
        npc->visual_heading = sfa_normalize_angle(npc->visual_heading);

        float ntarget_speed = sfa_speed_values[npc->speed_level];
        if (npc->current_speed < ntarget_speed) {
            npc->current_speed += accel * dt;
            if (npc->current_speed > ntarget_speed) npc->current_speed = ntarget_speed;
        } else if (npc->current_speed > ntarget_speed) {
            npc->current_speed -= accel * dt * 1.5f;
            if (npc->current_speed < ntarget_speed) npc->current_speed = ntarget_speed;
        }

        npc->x -= sinf(npc->heading) * npc->current_speed * dt;
        npc->z += cosf(npc->heading) * npc->current_speed * dt;

        if (npc->x >  SFA_ARENA_SIZE) npc->x =  SFA_ARENA_SIZE;
        if (npc->x < -SFA_ARENA_SIZE) npc->x = -SFA_ARENA_SIZE;
        if (npc->z >  SFA_ARENA_SIZE) npc->z =  SFA_ARENA_SIZE;
        if (npc->z < -SFA_ARENA_SIZE) npc->z = -SFA_ARENA_SIZE;
    }

    /* Update beam effects — track source/target positions */
    for (int i = 0; i < SFA_MAX_BEAMS; i++) {
        sfa_beam *b = &sfa.beams[i];
        if (!b->active) continue;
        b->timer -= dt;
        if (b->timer <= 0) { b->active = false; continue; }

        /* Update origin to follow source ship */
        sfa_ship *src = (b->source == -1) ? &sfa.player : &sfa.npcs[b->source];
        b->x0 = src->x;
        b->z0 = src->z;

        /* Update endpoint to follow target ship */
        sfa_ship *tgt = (b->target == -1) ? &sfa.player : &sfa.npcs[b->target];
        b->x1 = tgt->x;
        b->z1 = tgt->z;
    }

    /* Update torpedoes */
    for (int i = 0; i < SFA_MAX_TORPS; i++) {
        sfa_torpedo *tp = &sfa.torpedoes[i];
        if (!tp->active) continue;

        tp->x += tp->dx * dt;
        tp->z += tp->dz * dt;
        tp->timer -= dt;
        if (tp->timer <= 0) { tp->active = false; continue; }

        /* Collision with target */
        sfa_ship *victim = (tp->target == -1) ? &sfa.player : &sfa.npcs[tp->target];
        if (!victim->alive) { tp->active = false; continue; }

        float tdx = tp->x - victim->x;
        float tdz = tp->z - victim->z;
        if (tdx * tdx + tdz * tdz < SFA_TORP_HIT_RADIUS * SFA_TORP_HIT_RADIUS) {
            int si = sfa_shield_facing(victim, tp->x, tp->z);
            float pre_hull = victim->hull;
            sfa_apply_damage(victim, SFA_TORP_DAMAGE, si);
            sfa_spawn_explosion(tp->x, tp->z, 0.6f, 0xFF2244FF);
            tp->active = false;

            /* Track stats */
            if (tp->owner == -1) {
                sfa.stats.damage_dealt += SFA_TORP_DAMAGE;
                if (!victim->alive && pre_hull > 0)
                    sfa.stats.enemies_destroyed++;
            } else {
                sfa.stats.damage_taken += SFA_TORP_DAMAGE;
            }
        }
    }

    /* Update explosions */
    for (int i = 0; i < SFA_MAX_EXPLOSIONS; i++) {
        if (sfa.explosions[i].active) {
            sfa.explosions[i].timer -= dt;
            if (sfa.explosions[i].timer <= 0) sfa.explosions[i].active = false;
        }
    }

    /* Track combat time (only during active combat) */
    if (sfa.phase == SFA_PHASE_COMBAT)
        sfa.stats.combat_time += dt;

    /* Victory detection — all NPCs destroyed */
    if (sfa.phase == SFA_PHASE_COMBAT) {
        bool all_dead = true;
        for (int i = 0; i < sfa.npc_count; i++) {
            if (sfa.npcs[i].alive) { all_dead = false; break; }
        }
        if (all_dead) {
            sfa.phase = SFA_PHASE_VICTORY;
            sfa.phase_timer = 5.0f;
        }
    } else if (sfa.phase == SFA_PHASE_VICTORY) {
        sfa.phase_timer -= dt;
        if (sfa.phase_timer <= 0) {
            sfa.phase = SFA_PHASE_STATS;
        }
    }

    /* Smooth camera yaw toward selected target (Phantom-Nebula approach) */
    if (sfa.selected_npc >= 0 && sfa.selected_npc < sfa.npc_count) {
        sfa_ship *tgt = &sfa.npcs[sfa.selected_npc];
        float dx = tgt->x - s->x;
        float dz = tgt->z - s->z;
        /* Camera yaw faces toward target */
        float target_yaw = atan2f(-dx, dz);
        /* Shortest-path yaw interpolation with exponential decay */
        float cam_diff = sfa_normalize_angle(target_yaw - sfa.cam_target_yaw);
        float t = 1.0f - expf(-5.0f * dt);
        sfa.cam_target_yaw += cam_diff * t;
        sfa.cam_target_yaw = sfa_normalize_angle(sfa.cam_target_yaw);
    } else {
        /* No target — follow ship heading */
        float cam_diff = sfa_normalize_angle(s->visual_heading - sfa.cam_target_yaw);
        float t = 1.0f - expf(-5.0f * dt);
        sfa.cam_target_yaw += cam_diff * t;
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
                sr_vert_c(x1, sy + sh, z1, 1, 0, hull_top),
                sr_vert_c(x0, sy + sh, z0, 0, 0, hull_top),
                NULL, &mvp);
            /* Bottom face wedge */
            sr_draw_triangle(fb_ptr,
                sr_vert_c(0, sy - sh, sz, 0.5f, 0.5f, hull_bot),
                sr_vert_c(x0, sy - sh, z0, 0, 0, hull_bot),
                sr_vert_c(x1, sy - sh, z1, 1, 0, hull_bot),
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

    /* Star base colors (ABGR): 3 white, 1 blue */
    static const uint32_t star_colors[4] = {
        0xFFFFFFFF,  /* pure white */
        0xFFFFFFFF,  /* pure white */
        0xFFFFFFFF,  /* pure white */
        0xFFFFCC88,  /* light blue (ABGR: R=0x88, G=0xCC, B=0xFF) */
    };

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
                for (int si = 0; si < 1; si++) {
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
                    if (clip.w < 0.1f) continue;
                    float inv_w = 1.0f / clip.w;
                    float ndc_x = clip.x * inv_w;
                    float ndc_y = clip.y * inv_w;

                    int sx = (int)((ndc_x * 0.5f + 0.5f) * W);
                    int sy = (int)((1.0f - (ndc_y * 0.5f + 0.5f)) * H);

                    if (sx < 0 || sx >= W || sy < 0 || sy >= H) continue;

                    /* Pick tint and scale by brightness */
                    seed = seed * 1103515245u + 12345u;
                    uint32_t base = star_colors[(seed >> 8) % 4];
                    uint8_t br = (uint8_t)(((base      ) & 0xFF) * brightness);
                    uint8_t bg = (uint8_t)(((base >>  8) & 0xFF) * brightness);
                    uint8_t bb = (uint8_t)(((base >> 16) & 0xFF) * brightness);
                    uint32_t col = 0xFF000000 | ((uint32_t)bb << 16) | ((uint32_t)bg << 8) | br;

                    /* Close stars (< 40% range) = 2x2, far = 1x1 */
                    float dist_norm = dist2 / (view_range * view_range);
                    if (dist_norm < 0.16f && brightness > 0.25f) {
                        px[sy * W + sx] = col;
                        if (sx + 1 < W) px[sy * W + sx + 1] = col;
                        if (sy + 1 < H) px[(sy + 1) * W + sx] = col;
                        if (sx + 1 < W && sy + 1 < H) px[(sy + 1) * W + sx + 1] = col;
                    } else {
                        px[sy * W + sx] = col;
                    }
                }
            }
        }
    }
}

/* ── Arena boundary markers ──────────────────────────────────────── */

static void sfa_draw_arena_boundary(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                       float px, float pz) {
    float s = SFA_ARENA_SIZE;
    float y = -0.05f;
    float vis_dist = 40.0f;  /* distance at which border starts appearing */

    /* Only draw edges the player is near — compute distance to each edge */
    struct { float x0,z0,x1,z1; float player_dist; } edges[4] = {
        { -s, -s, -s,  s, px - (-s) },   /* left   (player dist = px + s) */
        {  s, -s,  s,  s, s - px     },   /* right  (player dist = s - px) */
        { -s, -s,  s, -s, pz - (-s) },   /* bottom (player dist = pz + s) */
        { -s,  s,  s,  s, s - pz     },   /* top    (player dist = s - pz) */
    };

    for (int i = 0; i < 4; i++) {
        if (edges[i].player_dist > vis_dist) continue;

        /* Brightness increases as player gets closer */
        float t = 1.0f - (edges[i].player_dist / vis_dist);
        if (t < 0) t = 0;
        uint8_t r_val = (uint8_t)(255.0f * t);
        uint8_t g_val = (uint8_t)(30.0f * t);
        uint32_t col = 0xFF000000 | (g_val << 8) | r_val;  /* ABGR red */

        /* Thicker line when closer */
        float w = 0.15f + t * 0.5f;

        float x0 = edges[i].x0, z0 = edges[i].z0;
        float x1 = edges[i].x1, z1 = edges[i].z1;

        if (x0 == x1) {
            /* Vertical edge (left or right) */
            float dx = (x0 < 0) ? w : -w;
            sr_draw_quad(fb_ptr,
                sr_vert_c(x0,    y, z0, 0,0, col), sr_vert_c(x0,    y, z1, 0,1, col),
                sr_vert_c(x0+dx, y, z1, 1,1, col), sr_vert_c(x0+dx, y, z0, 1,0, col),
                NULL, vp);
        } else {
            /* Horizontal edge (top or bottom) */
            float dz = (z0 < 0) ? w : -w;
            sr_draw_quad(fb_ptr,
                sr_vert_c(x0, y, z0,    0,0, col), sr_vert_c(x0, y, z0+dz, 0,1, col),
                sr_vert_c(x1, y, z0+dz, 1,1, col), sr_vert_c(x1, y, z0,    1,0, col),
                NULL, vp);
        }
    }
}

/* ── Target drawing & projection ─────────────────────────────────── */

/* Draw Klingon Bird of Prey — swept wings, central command pod, neck */
static void sfa_draw_target_ship(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                   float tx, float tz, float heading) {
    uint32_t hull_t = sfa_pal_abgr(30); /* 239063 dark green */
    uint32_t hull_s = sfa_pal_abgr(29); /* 165a4c darker green */
    uint32_t hull_b = sfa_pal_abgr(35); /* 374e4a darkest */
    uint32_t wing_t = sfa_pal_abgr(36); /* 547e64 olive green */
    uint32_t wing_s = sfa_pal_abgr(35); /* 374e4a */
    uint32_t wing_b = sfa_pal_abgr(34); /* 313638 near-black */
    uint32_t head_t = sfa_pal_abgr(31); /* 1ebc73 bright green */
    uint32_t head_s = sfa_pal_abgr(30);
    uint32_t gun_col = sfa_pal_abgr(15); /* e83b3b red — disruptor */

    sr_mat4 model = sr_mat4_mul(
        sr_mat4_translate(tx, 0.0f, tz),
        sr_mat4_rotate_y(-heading)
    );
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

/* ── 4x4 matrix inverse (cofactor expansion) ────────────────────── */

static bool sfa_mat4_invert(const sr_mat4 *m, sr_mat4 *out) {
    const float *s = &m->m[0][0];
    float inv[16];

    inv[0]  =  s[5]*s[10]*s[15] - s[5]*s[11]*s[14] - s[9]*s[6]*s[15]
             + s[9]*s[7]*s[14]  + s[13]*s[6]*s[11]  - s[13]*s[7]*s[10];
    inv[4]  = -s[4]*s[10]*s[15] + s[4]*s[11]*s[14]  + s[8]*s[6]*s[15]
             - s[8]*s[7]*s[14]  - s[12]*s[6]*s[11]  + s[12]*s[7]*s[10];
    inv[8]  =  s[4]*s[9]*s[15]  - s[4]*s[11]*s[13]  - s[8]*s[5]*s[15]
             + s[8]*s[7]*s[13]  + s[12]*s[5]*s[11]  - s[12]*s[7]*s[9];
    inv[12] = -s[4]*s[9]*s[14]  + s[4]*s[10]*s[13]  + s[8]*s[5]*s[14]
             - s[8]*s[6]*s[13]  - s[12]*s[5]*s[10]  + s[12]*s[6]*s[9];

    float det = s[0]*inv[0] + s[1]*inv[4] + s[2]*inv[8] + s[3]*inv[12];
    if (det > -1e-8f && det < 1e-8f) return false;

    inv[1]  = -s[1]*s[10]*s[15] + s[1]*s[11]*s[14]  + s[9]*s[2]*s[15]
             - s[9]*s[3]*s[14]  - s[13]*s[2]*s[11]  + s[13]*s[3]*s[10];
    inv[5]  =  s[0]*s[10]*s[15] - s[0]*s[11]*s[14]  - s[8]*s[2]*s[15]
             + s[8]*s[3]*s[14]  + s[12]*s[2]*s[11]  - s[12]*s[3]*s[10];
    inv[9]  = -s[0]*s[9]*s[15]  + s[0]*s[11]*s[13]  + s[8]*s[1]*s[15]
             - s[8]*s[3]*s[13]  - s[12]*s[1]*s[11]  + s[12]*s[3]*s[9];
    inv[13] =  s[0]*s[9]*s[14]  - s[0]*s[10]*s[13]  - s[8]*s[1]*s[14]
             + s[8]*s[2]*s[13]  + s[12]*s[1]*s[10]  - s[12]*s[2]*s[9];

    inv[2]  =  s[1]*s[6]*s[15]  - s[1]*s[7]*s[14]   - s[5]*s[2]*s[15]
             + s[5]*s[3]*s[14]  + s[13]*s[2]*s[7]    - s[13]*s[3]*s[6];
    inv[6]  = -s[0]*s[6]*s[15]  + s[0]*s[7]*s[14]    + s[4]*s[2]*s[15]
             - s[4]*s[3]*s[14]  - s[12]*s[2]*s[7]    + s[12]*s[3]*s[6];
    inv[10] =  s[0]*s[5]*s[15]  - s[0]*s[7]*s[13]    - s[4]*s[1]*s[15]
             + s[4]*s[3]*s[13]  + s[12]*s[1]*s[7]    - s[12]*s[3]*s[5];
    inv[14] = -s[0]*s[5]*s[14]  + s[0]*s[6]*s[13]    + s[4]*s[1]*s[14]
             - s[4]*s[2]*s[13]  - s[12]*s[1]*s[6]    + s[12]*s[2]*s[5];

    inv[3]  = -s[1]*s[6]*s[11]  + s[1]*s[7]*s[10]    + s[5]*s[2]*s[11]
             - s[5]*s[3]*s[10]  - s[9]*s[2]*s[7]     + s[9]*s[3]*s[6];
    inv[7]  =  s[0]*s[6]*s[11]  - s[0]*s[7]*s[10]    - s[4]*s[2]*s[11]
             + s[4]*s[3]*s[10]  + s[8]*s[2]*s[7]     - s[8]*s[3]*s[6];
    inv[11] = -s[0]*s[5]*s[11]  + s[0]*s[7]*s[9]     + s[4]*s[1]*s[11]
             - s[4]*s[3]*s[9]   - s[8]*s[1]*s[7]     + s[8]*s[3]*s[5];
    inv[15] =  s[0]*s[5]*s[10]  - s[0]*s[6]*s[9]     - s[4]*s[1]*s[10]
             + s[4]*s[2]*s[9]   + s[8]*s[1]*s[6]     - s[8]*s[2]*s[5];

    float inv_det = 1.0f / det;
    float *o = &out->m[0][0];
    for (int i = 0; i < 16; i++) o[i] = inv[i] * inv_det;
    return true;
}

/* Unproject a framebuffer pixel to a ray, intersect with Y=0 plane.
   Returns true if hit, writes world XZ to out_x/out_z. */
static bool sfa_screen_to_ground(float fb_x, float fb_y, int W, int H,
                                  const sr_mat4 *inv_vp, sr_vec3 eye,
                                  float *out_x, float *out_z) {
    /* NDC coords from framebuffer pixel */
    float ndc_x = (fb_x / (float)W) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (fb_y / (float)H) * 2.0f;

    /* Unproject near and far points */
    sr_vec4 near_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
    sr_vec4 far_clip  = { ndc_x, ndc_y,  1.0f, 1.0f };
    sr_vec4 near_w = sr_mat4_mul_v4(*inv_vp, near_clip);
    sr_vec4 far_w  = sr_mat4_mul_v4(*inv_vp, far_clip);

    if (near_w.w == 0 || far_w.w == 0) return false;
    float nw = 1.0f / near_w.w, fw = 1.0f / far_w.w;
    float nx = near_w.x * nw, ny = near_w.y * nw, nz = near_w.z * nw;
    float ffx = far_w.x * fw,  fy = far_w.y * fw,  fz = far_w.z * fw;

    /* Ray direction */
    float dx = ffx - nx, dy = fy - ny, dz = fz - nz;

    /* Intersect with Y=0 plane */
    if (dy > -1e-6f && dy < 1e-6f) return false; /* parallel to plane */
    float t = -ny / dy;
    if (t < 0) return false; /* behind camera */

    *out_x = nx + dx * t;
    *out_z = nz + dz * t;
    return true;
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

/* Compute screen-space bracket half-size by projecting ship bounding box corners.
   Returns the max pixel offset from center in X or Y. */
static int sfa_ship_screen_extent(const sr_mat4 *vp, float ship_x, float ship_z,
                                    float heading, int W, int H) {
    /* Target ship local-space bounding box (from vertex data) */
    static const float bbox_pts[][3] = {
        {-1.5f, -0.18f, -0.9f}, { 1.5f, -0.18f, -0.9f},
        {-1.5f,  0.15f, -0.9f}, { 1.5f,  0.15f, -0.9f},
        {-1.5f, -0.18f,  1.11f},{ 1.5f, -0.18f,  1.11f},
        {-1.5f,  0.15f,  1.11f},{ 1.5f,  0.15f,  1.11f},
    };

    float ch = cosf(-heading), sh = sinf(-heading);
    int cx, cy;
    float cw;
    if (!sfa_project_to_screen(vp, ship_x, 0.0f, ship_z, W, H, &cx, &cy, &cw))
        return 12;

    int max_off = 0;
    for (int i = 0; i < 8; i++) {
        /* Rotate local point by heading, translate to world */
        float lx = bbox_pts[i][0], ly = bbox_pts[i][1], lz = bbox_pts[i][2];
        float wx = ship_x + lx * ch - lz * sh;
        float wz = ship_z + lx * sh + lz * ch;
        float wy = ly;

        int sx, sy; float sw;
        if (!sfa_project_to_screen(vp, wx, wy, wz, W, H, &sx, &sy, &sw))
            continue;
        int dx = sx - cx, dy = sy - cy;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (dx > max_off) max_off = dx;
        if (dy > max_off) max_off = dy;
    }

    /* Add small padding, clamp */
    max_off += 3;
    if (max_off < 8) max_off = 8;
    if (max_off > 80) max_off = 80;
    return max_off;
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
    float best_dist = 1e9f;

    for (int i = 0; i < sfa.npc_count; i++) {
        sfa_ship *npc = &sfa.npcs[i];
        int scr_x, scr_y;
        float scr_w;
        if (!sfa_project_to_screen(vp, npc->x, 0.2f, npc->z, fb_w, fb_h, &scr_x, &scr_y, &scr_w))
            continue;

        /* Compute bracket size from projected ship bounding box */
        int bracket_half = sfa_ship_screen_extent(vp, npc->x, npc->z,
                                                    npc->visual_heading, fb_w, fb_h);

        /* Check if mouse is inside the bracket area */
        float ddx = sfa.mouse_fb_x - (float)scr_x;
        float ddy = sfa.mouse_fb_y - (float)scr_y;
        float pixel_dist = sqrtf(ddx*ddx + ddy*ddy);
        if (pixel_dist <= (float)bracket_half && pixel_dist < best_dist) {
            best_dist = pixel_dist;
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

    /* Target heading line — shown when ship is rotating */
    float vis_h = s->visual_heading;
    {
        float angle_diff = sfa_normalize_angle(s->target_heading - s->heading);
        if (angle_diff > 0.01f || angle_diff < -0.01f) {
            float th = s->target_heading;
            int steps = sr_radius - 6;
            uint32_t tgt_col = 0xFF00AAFF; /* orange target line */
            for (int i = 0; i < steps; i++) {
                float t = (float)i / (float)steps;
                int lx = scx + (int)(sinf(th) * t * (sr_radius - 6));
                int ly = scy - (int)(cosf(th) * t * (sr_radius - 6));
                if (lx >= 0 && lx < W && ly >= 0 && ly < H)
                    px[ly * W + lx] = tgt_col;
            }
            /* Target dot at end */
            int tgt_x = scx + (int)(sinf(th) * (sr_radius - 6));
            int tgt_y = scy - (int)(cosf(th) * (sr_radius - 6));
            sfa_draw_circle(px, W, H, tgt_x, tgt_y, 2, tgt_col);
        }
    }

    /* If touch steering active, show touch target too */
    if (sfa.touch_steering) {
        int tgt_x = scx + (int)(sinf(sfa.touch_steer_angle) * (sr_radius - 6));
        int tgt_y = scy - (int)(cosf(sfa.touch_steer_angle) * (sr_radius - 6));
        sfa_draw_circle(px, W, H, tgt_x, tgt_y, 2, SFA_HUD_WARN);
    }

    /* Blue arrow icon centered on dial, rotated with 2D rotation matrix */
    {
        /* Arrow shape in local space (pointing UP, centered at origin) */
        /* Chevron: nose, left wing, tail notch, right wing */
        float arrow_pts[][2] = {
            {  0.0f, -10.0f },  /* 0: nose (top) */
            { -6.0f,   4.0f },  /* 1: left wing */
            {  0.0f,   1.0f },  /* 2: tail notch */
            {  6.0f,   4.0f },  /* 3: right wing */
        };
        int npts = 4;

        /* 2D rotation matrix: rotate by vis_h */
        /* dial convention: +heading rotates clockwise on dial (sin,cos) */
        float c = cosf(vis_h), si = sinf(vis_h);

        /* Transform all points: x' = x*cos - y*sin, y' = x*sin + y*cos */
        int sx[4], sy[4];
        for (int i = 0; i < npts; i++) {
            float lx = arrow_pts[i][0], ly = arrow_pts[i][1];
            float rx = lx * c - ly * si;
            float ry = lx * si + ly * c;
            sx[i] = scx + (int)(rx);
            sy[i] = scy + (int)(ry);
        }

        uint32_t arrow_col = 0xFFFF8833; /* ABGR: blue */

        /* Fill triangle: nose(0), left(1), notch(2) */
        for (int i = 0; i <= 30; i++) {
            float t = (float)i / 30.0f;
            /* Edge nose→left */
            int ax = sx[0] + (int)((sx[1] - sx[0]) * t);
            int ay = sy[0] + (int)((sy[1] - sy[0]) * t);
            /* Edge nose→notch */
            int bx = sx[0] + (int)((sx[2] - sx[0]) * t);
            int by = sy[0] + (int)((sy[2] - sy[0]) * t);
            int len = (int)(sqrtf((float)((bx-ax)*(bx-ax)+(by-ay)*(by-ay)))) + 1;
            for (int j = 0; j <= len; j++) {
                float u = (len > 0) ? (float)j / (float)len : 0;
                int px2 = ax + (int)((bx - ax) * u);
                int py2 = ay + (int)((by - ay) * u);
                if (px2 >= 0 && px2 < W && py2 >= 0 && py2 < H)
                    px[py2 * W + px2] = arrow_col;
            }
        }
        /* Fill triangle: nose(0), notch(2), right(3) */
        for (int i = 0; i <= 30; i++) {
            float t = (float)i / 30.0f;
            int ax = sx[0] + (int)((sx[3] - sx[0]) * t);
            int ay = sy[0] + (int)((sy[3] - sy[0]) * t);
            int bx = sx[0] + (int)((sx[2] - sx[0]) * t);
            int by = sy[0] + (int)((sy[2] - sy[0]) * t);
            int len = (int)(sqrtf((float)((bx-ax)*(bx-ax)+(by-ay)*(by-ay)))) + 1;
            for (int j = 0; j <= len; j++) {
                float u = (len > 0) ? (float)j / (float)len : 0;
                int px2 = ax + (int)((bx - ax) * u);
                int py2 = ay + (int)((by - ay) * u);
                if (px2 >= 0 && px2 < W && py2 >= 0 && py2 < H)
                    px[py2 * W + px2] = arrow_col;
            }
        }
    }

    /* North (heading=0) indicator — yellow line, shown while rotating */
    {
        float angle_diff = sfa_normalize_angle(s->target_heading - s->heading);
        if (angle_diff > 0.01f || angle_diff < -0.01f) {
            /* Draw yellow "N" line at heading 0 on the dial (top = 0) */
            uint32_t yellow = 0xFF00FFFF; /* ABGR yellow */
            int n_end_x = scx;
            int n_end_y = scy - (sr_radius - 2);
            for (int i = 0; i < sr_radius - 6; i++) {
                float t = (float)i / (float)(sr_radius - 6);
                int lx = scx;
                int ly = scy - (int)(t * (sr_radius - 2));
                if (lx >= 0 && lx < W && ly >= 0 && ly < H)
                    px[ly * W + lx] = yellow;
            }
            sr_draw_text_shadow(px, W, H, n_end_x - 2, n_end_y - 8,
                                 "N", yellow, SFA_HUD_SHADOW);
        }
    }

    /* Enemy dots on minimap — plot world XZ relative to player (no rotation, dial is fixed) */
    {
        float max_range = 60.0f;
        float map_r = (float)(sr_radius - 4); /* usable pixel radius */
        for (int i = 0; i < sfa.npc_count; i++) {
            sfa_ship *npc = &sfa.npcs[i];
            if (!npc->alive) continue;
            float dx = npc->x - sfa.player.x;
            float dz = npc->z - sfa.player.z;
            float dist = sqrtf(dx * dx + dz * dz);

            /* Unit direction * scaled distance, clamped to minimap radius */
            float r = (dist < 0.01f) ? 0.0f : fminf(dist / max_range, 1.0f) * map_r;
            float ux = (dist < 0.01f) ? 0.0f : dx / dist;
            float uz = (dist < 0.01f) ? 0.0f : dz / dist;

            int ex = scx - (int)(ux * r);  /* minimap right = world -X (heading 90) */
            int ey = scy - (int)(uz * r);

            uint32_t dot_col = (i == sfa.selected_npc)
                ? 0xFFFF6464 : 0xFF4444CC;
            int dot_size = (i == sfa.selected_npc) ? 3 : 2;
            sfa_draw_circle(px, W, H, ex, ey, dot_size, dot_col);
        }
    }

    /* Center dot */
    sfa_draw_circle(px, W, H, scx, scy, 2, 0x40FFFFFF);

    /* Degree labels around dial: 0 (top), 90 (right), 180 (bottom), 270 (left) */
    {
        int lr = sr_radius + 5;
        sr_draw_text_shadow(px, W, H, scx - 3, scy - lr - 4, "0", 0xFF888888, SFA_HUD_SHADOW);
        sr_draw_text_shadow(px, W, H, scx + lr, scy - 3, "90", 0xFF888888, SFA_HUD_SHADOW);
        sr_draw_text_shadow(px, W, H, scx - 6, scy + lr - 2, "180", 0xFF888888, SFA_HUD_SHADOW);
        sr_draw_text_shadow(px, W, H, scx - lr - 12, scy - 3, "270", 0xFF888888, SFA_HUD_SHADOW);
    }

    /* Label */
    sr_draw_text_shadow(px, W, H, scx - 12, scy - sr_radius - 14,
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
        else if (i == SFA_NUM_SPEEDS - 1) snprintf(label, sizeof(label), "FULL");
        else snprintf(label, sizeof(label), "%d/4", i);
        sr_draw_text_shadow(px, W, H, bx + 4, by + 5, label, fg, SFA_HUD_SHADOW);
    }

}

/* ── HUD: Weapon bars (bottom-center, combined button + cooldown) ── */

static void sfa_draw_weapon_bars(uint32_t *px, int W, int H, sfa_ship *s) {
    int bar_h = 20;
    int gap = 4;
    int pad = 4; /* horizontal padding inside bar */

    /* Build labels first so we can measure them */
    char ph_label[20], tp_label[20];

    float ph_pct = 1.0f - (s->phaser_cooldown / SFA_PHASER_COOLDOWN);
    if (ph_pct > 1.0f) ph_pct = 1.0f;
    if (ph_pct < 0.0f) ph_pct = 0.0f;
    bool ph_rdy = ph_pct >= 1.0f;

    if (ph_rdy)
        snprintf(ph_label, sizeof(ph_label), "PHSR [SPC]");
    else
        snprintf(ph_label, sizeof(ph_label), "PHSR %.1f", s->phaser_cooldown);

    float tp_pct = 1.0f - (s->torpedo_cooldown / SFA_TORP_COOLDOWN);
    if (tp_pct > 1.0f) tp_pct = 1.0f;
    if (tp_pct < 0.0f) tp_pct = 0.0f;
    bool tp_rdy = tp_pct >= 1.0f && s->torpedoes_remaining > 0;

    if (s->torpedoes_remaining <= 0)
        snprintf(tp_label, sizeof(tp_label), "TORP EMPTY");
    else if (tp_rdy)
        snprintf(tp_label, sizeof(tp_label), "TORP [F] x%d", s->torpedoes_remaining);
    else
        snprintf(tp_label, sizeof(tp_label), "TORP x%d %.1f", s->torpedoes_remaining, s->torpedo_cooldown);

    /* Size bars to fit their text */
    int ph_w = (int)strlen(ph_label) * 6 + pad * 2;
    int tp_w = (int)strlen(tp_label) * 6 + pad * 2;
    int total_w = ph_w + gap + tp_w;
    int x0 = (W - total_w) / 2;
    int y = H - bar_h - 4;

    /* ── Phaser bar ── */
    {
        int x = x0;
        int fill = (int)(ph_w * ph_pct);
        sfa_draw_rect(px, W, H, x, y, x + ph_w, y + bar_h, SFA_HUD_BG);
        uint32_t fill_col = ph_rdy ? 0xFF44FF44 : 0xFF44AACC;
        if (fill > 0) sfa_draw_rect(px, W, H, x, y, x + fill, y + bar_h, fill_col);
        sr_draw_text_shadow(px, W, H, x + pad, y + 6, ph_label,
                             ph_rdy ? 0xFFFFFFFF : 0xFFCCCCCC, SFA_HUD_SHADOW);
    }

    /* ── Torpedo bar ── */
    {
        int x = x0 + ph_w + gap;
        int fill = (int)(tp_w * tp_pct);
        sfa_draw_rect(px, W, H, x, y, x + tp_w, y + bar_h, SFA_HUD_BG);
        uint32_t fill_col = tp_rdy ? 0xFF4466FF : 0xFF44AACC;
        if (fill > 0) sfa_draw_rect(px, W, H, x, y, x + fill, y + bar_h, fill_col);
        sr_draw_text_shadow(px, W, H, x + pad, y + 6, tp_label,
                             tp_rdy ? 0xFFFFFFFF : 0xFFCCCCCC, SFA_HUD_SHADOW);
    }

    /* Target status warning (centered above bars) */
    {
        const char *warn = NULL;
        uint32_t warn_col = 0xFF4466FF;
        if (sfa.selected_npc < 0) {
            warn = "NO TARGET [TAB]";
            warn_col = 0xFF6666AA;
        } else {
            sfa_ship *tgt = &sfa.npcs[sfa.selected_npc];
            if (!tgt->alive) {
                warn = "DESTROYED";
                warn_col = 0xFF4444CC;
            } else {
                float dx = tgt->x - s->x;
                float dz = tgt->z - s->z;
                float dist = sqrtf(dx * dx + dz * dz);
                if (dist > SFA_PHASER_RANGE) {
                    warn = "OUT OF RANGE";
                } else if (!sfa_in_weapon_arc(s, tgt->x, tgt->z, SFA_PHASER_ARC)) {
                    warn = "OUT OF ARC";
                    warn_col = 0xFF44AAFF;
                }
            }
        }
        if (warn) {
            int tw = (int)strlen(warn) * 6;
            sr_draw_text_shadow(px, W, H, (W - tw) / 2, y - 12, warn,
                                 warn_col, SFA_HUD_SHADOW);
        }
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

    /* Player health bar (top-right, left of MENU button) */
    {
        float total_hp = s->hull;
        for (int i = 0; i < 6; i++) total_hp += s->shields[i];
        float hp_pct = total_hp / 700.0f;
        if (hp_pct < 0) hp_pct = 0;
        if (hp_pct > 1) hp_pct = 1;

        int bar_w = 80, bar_h = 7;
        int bx = W - bar_w - 36;
        int by = 3;

        sfa_draw_rect(px, W, H, bx, by, bx + bar_w, by + bar_h, 0xC0000000);
        int fill = (int)(bar_w * hp_pct);
        uint32_t hp_col = hp_pct > 0.6f ? 0xFF44CC44
                        : hp_pct > 0.3f ? 0xFF44CCCC
                        :                  0xFF4444CC;
        if (fill > 0)
            sfa_draw_rect(px, W, H, bx, by, bx + fill, by + bar_h, hp_col);

        /* Hull percentage label inside bar */
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)(hp_pct * 100));
        sr_draw_text_shadow(px, W, H, bx + 2, by - 1, buf,
                             SFA_HUD_BRIGHT, SFA_HUD_SHADOW);
    }

    /* Shield hex display (bottom-left) */
    sfa_draw_shield_hud(px, W, H, s, 45, H - 100);

    /* Mobile virtual controls */
    sfa_draw_mobile_controls(px, W, H, s);

    /* Weapon bars (bottom-center) */
    sfa_draw_weapon_bars(px, W, H, s);

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

    /* Check MENU button */
    int mbx = FB_WIDTH - 32, mby = 3, mbw = 30, mbh = 11;
    if (fx >= mbx && fx <= mbx + mbw && fy >= mby && fy <= mby + mbh) {
        app_state = STATE_MENU;
        return true;
    }

    /* Check minimap click — select target by clicking its dot */
    {
        int scx = SFA_VCTRL_STEER_CX;
        int scy = SFA_VCTRL_STEER_CY;
        int sr = SFA_VCTRL_STEER_R;
        float mdx = fx - scx, mdy = fy - scy;
        if (mdx * mdx + mdy * mdy < (float)(sr * sr)) {
            float max_range = 60.0f;
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
                sfa.selected_npc = (sfa.selected_npc == best) ? -1 : best;
                return true;
            }
        }
    }

    /* Check weapon fire bars first (bottom-center, dynamic sizing) */
    {
        int bar_h = 20, gap = 4, pad = 4;
        char ph_l[20];
        if (sfa.player.phaser_cooldown <= 0)
            snprintf(ph_l, sizeof(ph_l), "PHSR [SPC]");
        else
            snprintf(ph_l, sizeof(ph_l), "PHSR %.1f", sfa.player.phaser_cooldown);
        int ph_w = (int)strlen(ph_l) * 6 + pad * 2;

        char tp_l[20];
        if (sfa.player.torpedoes_remaining <= 0)
            snprintf(tp_l, sizeof(tp_l), "TORP EMPTY");
        else if (sfa.player.torpedo_cooldown <= 0)
            snprintf(tp_l, sizeof(tp_l), "TORP [F] x%d", sfa.player.torpedoes_remaining);
        else
            snprintf(tp_l, sizeof(tp_l), "TORP x%d %.1f", sfa.player.torpedoes_remaining, sfa.player.torpedo_cooldown);
        int tp_w = (int)strlen(tp_l) * 6 + pad * 2;

        int total_w = ph_w + gap + tp_w;
        int wx0 = (FB_WIDTH - total_w) / 2;
        int wy = FB_HEIGHT - bar_h - 4;

        /* PHASER bar */
        if (fx >= wx0 && fx <= wx0 + ph_w && fy >= wy && fy <= wy + bar_h) {
            if (sfa.selected_npc >= 0 && sfa.selected_npc < sfa.npc_count) {
                sfa_ship *tgt = &sfa.npcs[sfa.selected_npc];
                if (tgt->alive) sfa_fire_phaser(&sfa.player, tgt, -1, sfa.selected_npc);
            }
            return true;
        }
        /* TORP bar */
        int tx = wx0 + ph_w + gap;
        if (fx >= tx && fx <= tx + tp_w && fy >= wy && fy <= wy + bar_h) {
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

    /* If hovering an NPC, select/deselect it */
    if (sfa.hovered_npc >= 0) {
        if (sfa.selected_npc == sfa.hovered_npc)
            sfa.selected_npc = -1;  /* deselect */
        else
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

    /* Draw NPC ships (skip dead) */
    for (int i = 0; i < sfa.npc_count; i++) {
        if (!sfa.npcs[i].alive) continue;
        sfa_draw_target_ship(fb_ptr, &vp, sfa.npcs[i].x, sfa.npcs[i].z,
                             sfa.npcs[i].visual_heading);
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
                                                        npc->visual_heading, W, H);

            if (!npc->alive) {
                /* Dead: show dim X marker */
                sr_draw_text_shadow(px, W, H, scr_x - 3, scr_y - 4,
                                     "X", 0xFF444444, SFA_HUD_SHADOW);
                continue;
            }

            /* Health bar above brackets — shows total integrity (shields + hull) */
            {
                int hbar_w = bracket_half * 2;
                int hbar_h = 2;
                int hx = scr_x - bracket_half;
                int hy = scr_y - bracket_half - 5;
                /* Total: 6 shields * 100 + 100 hull = 700 max */
                float total_hp = npc->hull;
                for (int si = 0; si < 6; si++) total_hp += npc->shields[si];
                float hp_pct = total_hp / 700.0f;
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

#endif /* SR_SCENE_SPACE_FLEET_H */
