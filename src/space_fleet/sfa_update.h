/*  sfa_update.h — Game update loop and NPC AI.
 *  Header-only. Depends on sfa_types.h, sfa_combat.h. */
#ifndef SFA_UPDATE_H
#define SFA_UPDATE_H

static void sfa_update(float dt) {
    sfa_ship *s = &sfa.player;

    /* Ship class multipliers */
    int pcls = s->ship_class;
    if (pcls < 0 || pcls >= SHIP_CLASS_COUNT) pcls = SHIP_CLASS_CRUISER;
    float p_turn_mult = ship_classes[pcls].turn_mult;
    float p_speed_mult = ship_classes[pcls].speed_mult;

    /* Apply continuous keyboard/touch steering (positive heading = screen-right) */
    float p_turn_rate = SFA_TURN_RATE * p_turn_mult;
    if (sfa_key_left  || sfa.touch_turn_left)  s->target_heading -= p_turn_rate * dt;
    if (sfa_key_right || sfa.touch_turn_right) s->target_heading += p_turn_rate * dt;

    /* Normalize target heading */
    s->target_heading = sfa_normalize_angle(s->target_heading);

    /* Rotate toward target heading */
    float diff = sfa_normalize_angle(s->target_heading - s->heading);
    float max_turn = p_turn_rate * dt;
    if (diff > max_turn) diff = max_turn;
    else if (diff < -max_turn) diff = -max_turn;
    s->heading += diff;
    s->heading = sfa_normalize_angle(s->heading);

    /* Smooth visual heading */
    float vdiff = sfa_normalize_angle(s->heading - s->visual_heading);
    s->visual_heading += vdiff * 8.0f * dt;
    s->visual_heading = sfa_normalize_angle(s->visual_heading);

    /* Accelerate toward target speed */
    float target_speed = sfa_speed_values[s->speed_level] * p_speed_mult;
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

        /* NPC physics — scaled by ship class */
        int ncls = npc->ship_class;
        if (ncls < 0 || ncls >= SHIP_CLASS_COUNT) ncls = SHIP_CLASS_DESTROYER;
        float n_turn_rate = SFA_TURN_RATE * ship_classes[ncls].turn_mult;
        float n_speed_mult = ship_classes[ncls].speed_mult;

        float ndiff = sfa_normalize_angle(npc->target_heading - npc->heading);
        float nmax = n_turn_rate * dt;
        if (ndiff > nmax) ndiff = nmax;
        else if (ndiff < -nmax) ndiff = -nmax;
        npc->heading += ndiff;
        npc->heading = sfa_normalize_angle(npc->heading);

        float nvdiff = sfa_normalize_angle(npc->heading - npc->visual_heading);
        npc->visual_heading += nvdiff * 8.0f * dt;
        npc->visual_heading = sfa_normalize_angle(npc->visual_heading);

        float ntarget_speed = sfa_speed_values[npc->speed_level] * n_speed_mult;
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

#endif /* SFA_UPDATE_H */
