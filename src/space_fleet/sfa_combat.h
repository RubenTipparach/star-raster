/*  sfa_combat.h — Initialization, math helpers, and combat mechanics.
 *  Header-only. Depends on sfa_types.h. */
#ifndef SFA_COMBAT_H
#define SFA_COMBAT_H

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

    /* Player ship — stats from ship class if in campaign */
    sfa_init_ship(&sfa.player, 0.0f, 0.0f, 0.0f, 0, 0);
    if (campaign.campaign_active) {
        sfa.player.ship_class = campaign.player_ship_class;
        const ship_class_stats *psc = &ship_classes[campaign.player_ship_class];
        sfa.player.hull = (float)psc->hull_max;
        for (int i = 0; i < 6; i++)
            sfa.player.shields[i] = psc->shield_max;
    } else {
        sfa.player.ship_class = SHIP_CLASS_CRUISER; /* default sandbox */
    }

    /* Set up NPCs from campaign encounter or default */
    if (campaign.campaign_active && campaign.encounter_enemy_count > 0) {
        sfa.npc_count = campaign.encounter_enemy_count;
        for (int i = 0; i < sfa.npc_count; i++) {
            float angle = SFA_TWO_PI * (float)i / (float)sfa.npc_count;
            float dist = 15.0f + 8.0f * (float)i;
            sfa_init_ship(&sfa.npcs[i],
                          dist * cosf(angle), dist * sinf(angle),
                          angle + SFA_PI, 0, 0);
            int cls = campaign.encounter_enemy_classes[i];
            sfa.npcs[i].ship_class = cls;
            sfa.npcs[i].is_boss = campaign.encounter_enemy_is_boss[i];
            if (cls <= SHIP_CLASS_FRIGATE)
                sfa.npcs[i].speed_level = SFA_SPEED_HALF;
            else
                sfa.npcs[i].speed_level = SFA_SPEED_QUARTER;
            const ship_class_stats *esc = &ship_classes[cls];
            sfa.npcs[i].hull = (float)esc->hull_max;
            for (int j = 0; j < 6; j++)
                sfa.npcs[i].shields[j] = esc->shield_max;
        }
    } else {
        /* Default sandbox: 2 Klingon ships */
        sfa.npc_count = 2;
        sfa_init_ship(&sfa.npcs[0], 15.0f, 20.0f, SFA_PI * 0.75f, 0, 0);
        sfa.npcs[0].speed_level = SFA_SPEED_QUARTER;
        sfa.npcs[0].ship_class = SHIP_CLASS_DESTROYER;
        sfa_init_ship(&sfa.npcs[1], 30.0f, 20.0f, SFA_PI * 0.25f, 0, 0);
        sfa.npcs[1].speed_level = SFA_SPEED_HALF;
        sfa.npcs[1].ship_class = SHIP_CLASS_FRIGATE;
    }

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

#endif /* SFA_COMBAT_H */
