# Space Fleet Assault — Game Design Reference Document

## Overview

**Space Fleet Assault** is a real-time starship combat game inspired by *Star Trek: Tactical Assault* (2006, PSP/DS), a top-down 2D ship combat game developed by Quicksilver Software. The original emphasized tactical maneuvering, shield management, and energy allocation over twitch reflexes.

This document breaks down the core gameplay systems for reference when designing Space Fleet Assault on the StarRaster engine.

---

## 1. Core Gameplay Loop

```
Mission Briefing
    ↓
Warp to Encounter Area
    ↓
Hail / Dialogue (optional branching)
    ↓
Set Alert Condition → Red Alert (activates shields & weapons)
    ↓
┌─────────────────────────────────┐
│      COMBAT LOOP                │
│                                 │
│  1. Maneuver ship (heading +    │
│     speed)                      │
│  2. Rotate strongest shields    │
│     toward enemy                │
│  3. Fire weapons when facing    │
│     + recharged                 │
│  4. Manage energy (shields vs   │
│     weapons vs emergency)       │
│  5. Repeat until victory or     │
│     retreat                     │
└─────────────────────────────────┘
    ↓
Mission Debrief → Medal Rank → Crew Upgrade Points
    ↓
Ship Unlock / Next Mission
```

---

## 2. Ship Movement & Navigation

### 2.1 Movement Model
- **2D plane**: Ships move on a flat 2D plane (top-down perspective). No vertical axis.
- **Heading**: Ships have a continuous heading (0–360°). The player sets a desired heading and the ship rotates toward it at a turn rate determined by the ship class.
- **Speed**: Discrete speed levels (e.g., stop, 1/4 impulse, 1/2 impulse, full impulse). Larger ships accelerate and decelerate more slowly.
- **Emergency Turn**: Consumes emergency power to execute a rapid heading change, useful for dodging torpedoes or repositioning shields.

### 2.2 Warp Drive
- Used between encounters to travel to new locations within a mission.
- Not used during combat — serves as a scene transition mechanic.
- In some missions, warping away from combat is a valid (or required) strategy.

---

## 3. Shield System

### 3.1 Shield Facings
Shields are divided into **six arcs** around the ship:

```
         Forward
        ┌───────┐
       / \  F  / \
      /   \   /   \
     / FL  \ / FR  \
    ├───────●───────┤
     \ AL  / \ AR  /
      \   /   \   /
       \ /  A  \ /
        └───────┘
          Aft

  F  = Forward
  FL = Forward-Left
  FR = Forward-Right
  AL = Aft-Left
  AR = Aft-Right
  A  = Aft (Rear)
```

### 3.2 Shield Mechanics
- Each shield facing has its own **hit points** (strength value).
- Incoming weapon fire damages the shield facing it hits.
- When a shield facing reaches 0, weapons strike the **hull directly**, causing critical system damage.
- **Shield Recharging**: The player can manually allocate energy to recharge shields. This diverts power from weapon recharge, creating a core trade-off.
- Ships can rotate to present their strongest shield facing to the enemy — this is the fundamental tactical maneuver.

### 3.3 Shield Trade-off
- Recharging shields = slower weapon recharge.
- Recharging weapons = shields degrade over time under fire.
- The player must constantly decide: *attack harder or stay alive longer?*

---

## 4. Weapons Systems

### 4.1 Weapon Types

| Weapon            | Type       | Range   | Damage   | Recharge | Notes |
|-------------------|-----------|---------|----------|----------|-------|
| **Phasers**       | Beam      | Medium  | Moderate | Fast     | Instant hit, good for shield stripping |
| **Photon Torpedoes** | Projectile | Long | High     | Slow     | Must be aimed, can miss at range |
| **Disruptors**    | Beam      | Medium  | Moderate | Fast     | Klingon/Romulan equivalent of phasers |
| **Plasma Torpedoes** | Projectile | Short | Very High | Very Slow | Gorn specialty, devastating but short range |

### 4.2 Weapon Arcs
- Weapons are mounted at specific locations on the ship and can only fire within their **arc of coverage**.
- Forward-mounted weapons (torpedoes) require the ship to face the enemy.
- Broadside-mounted weapons (some phasers) can fire to the sides.
- The player must maneuver to bring the right weapons to bear on the target.

### 4.3 Weapon Recharge
- All weapons have a **recharge timer** after firing.
- Weapons are NOT instantly available — you must wait for recharge.
- **Overcharging**: Spending emergency power to boost weapon damage significantly. The fastest way to break through shields, but consumes your limited emergency reserve.

### 4.4 Firing
- Player selects weapons individually or in groups.
- Weapons only fire if:
  1. The target is within the weapon's **arc**.
  2. The target is within **range**.
  3. The weapon has **recharged**.

---

## 5. Energy & Power Management

### 5.1 Energy Systems
The ship has several interconnected energy pools:

| System              | Function |
|---------------------|----------|
| **Main Power**      | Passive pool that regenerates over time. Feeds shields and weapons. |
| **Shield Energy**   | Dedicated to shield recharge. Drawing more = slower weapon regen. |
| **Weapon Energy**   | Dedicated to weapon recharge. Drawing more = slower shield regen. |
| **Emergency Power** | Limited reserve for special actions. Regenerates very slowly. |

### 5.2 Emergency Power Uses
- **Overcharge Weapons**: Boost damage output for next salvo.
- **Emergency Turn**: Rapid heading change.
- **Shield Recharge Burst**: Quickly restore a depleted shield facing.
- **Cloaking Device** (Romulans/Klingons only): Become invisible, but cannot fire while cloaked.

### 5.3 The Core Tension
Energy is a zero-sum game. Every unit of power allocated to offense is a unit not allocated to defense. The player is constantly making micro-decisions about power distribution, which creates the strategic depth of combat.

---

## 6. Alert Conditions

| Condition     | Effect |
|---------------|--------|
| **Green Alert** | Peacetime. Shields and weapons offline. Full power to engines and sensors. |
| **Yellow Alert** | Caution. Shields begin charging. Weapons on standby. |
| **Red Alert**   | Combat. Shields and weapons fully online. Required before engaging. |

Transitioning from Green → Red takes time. Being caught at Green Alert by a surprise attack is dangerous — shields and weapons need seconds to come online.

---

## 7. Ship Classes & Progression

### 7.1 Federation Ships (Campaign Progression)
Players start in a small frigate and earn promotions to larger vessels:

1. **Frigate** — Small, fast, lightly armed. Few shield hit points.
2. **Destroyer** — Balanced speed and firepower.
3. **Light Cruiser** — More weapons, stronger shields, slower turn rate.
4. **Heavy Cruiser** — Powerful but sluggish.
5. **Constitution-class** — The iconic Enterprise-style ship. Strong all-around.

### 7.2 Faction Ships
Five factions, each with distinct characteristics (~20 ships total):

| Faction       | Strengths | Weaknesses | Special |
|---------------|-----------|------------|---------|
| **Federation** | Balanced, shield recharge ability | No cloaking | Shield recharge crew skill |
| **Klingon**    | Strong forward weapons, aggressive | Weaker aft shields | Cloaking device (later era) |
| **Romulan**    | Cloaking device, ambush tactics | Fragile when decloaked | Plasma torpedoes on some ships |
| **Gorn**       | Devastating plasma torpedoes | Slow, short weapon range | Plasma torpedo specialty |
| **Orion**      | Fast, maneuverable | Weak shields and hull | Speed advantage |

### 7.3 Ship Stats
Each ship has ratings in:
- **Hull Hit Points** — Total structural integrity.
- **Shield Strength** — Per-facing shield HP.
- **Turn Rate** — How fast the ship can change heading.
- **Speed** — Maximum impulse speed.
- **Weapon Mounts** — Number and type of weapon hardpoints.
- **Emergency Power Capacity** — Size of the emergency reserve.

---

## 8. Crew Upgrade System

### 8.1 Earning Points
After each mission, performance is graded with a **medal rank** (Bronze, Silver, Gold). Higher ranks award more **crew upgrade points**.

### 8.2 Upgrade Categories
Points are allocated to bridge crew members, improving specific subsystems:

| Crew Role        | Upgrades |
|------------------|----------|
| **Helm**         | Turn rate, speed, emergency turn efficiency |
| **Tactical**     | Weapon damage, accuracy, recharge speed |
| **Engineering**  | Damage control (auto-repair), power efficiency |
| **Science**      | Sensor range, shield recharge rate (Federation only) |
| **Security**     | Boarding defense, hull integrity |

### 8.3 Strategy
- Crew upgrades **persist across ship changes** — your trained crew transfers to new vessels.
- Players specialize their crew to match their playstyle:
  - **Aggressive**: Max weapon recharge + accuracy → constant firepower.
  - **Defensive**: Max shield recharge + damage control → outlast the enemy.
  - **Tactical**: Balanced approach with helm upgrades for superior positioning.

---

## 9. Combat Strategies

### 9.1 Circle Strafing
Orbit the enemy ship at medium range, keeping your strongest shields facing them while bringing broadside weapons to bear. Classic Federation tactic.

### 9.2 Head-On Pass
Charge directly at the enemy, fire forward torpedoes at close range for maximum damage, then turn away. High risk, high reward. Favored by Klingon players.

### 9.3 Kiting
Maintain maximum range, using phasers to wear down shields while staying out of torpedo range. Effective against slow ships (Gorn).

### 9.4 Cloak & Strike
Romulan/Klingon-specific. Cloak to reposition, decloak at point-blank range, unleash full weapons, then re-cloak. Requires careful emergency power management.

### 9.5 Shield Rotation
When one shield facing is low, rotate the ship to present a fresh shield. Keep cycling through facings to spread damage across all six arcs. Time shield recharges between rotations.

### 9.6 Overcharge Alpha Strike
Save emergency power, close to optimal range, overcharge all weapons, and fire everything at a single shield facing. Can destroy a ship in one devastating volley if the shield drops.

---

## 10. Mission Structure

### 10.1 Campaign Flow
- ~15-20 missions per campaign (Federation and Klingon).
- Missions include: patrol, escort, intercept, defend station, ambush, boss encounters.
- **Branching dialogue**: Hailing enemies before combat can lead to alternative resolutions (diplomacy, surrender, ambush).

### 10.2 Mission Types

| Type | Description |
|------|-------------|
| **Patrol** | Travel between waypoints, engage hostiles encountered. |
| **Escort** | Protect a friendly ship from attackers. |
| **Intercept** | Hunt down and destroy a specific target. |
| **Defense** | Protect a starbase or planet from waves of attackers. |
| **Stealth** | Avoid detection or use cloaking to reach an objective. |
| **Boss** | Face a powerful enemy vessel (often a capital ship). |

### 10.3 Multiplayer Modes
- **Skirmish**: 1v1 or 2v2 ship combat with any faction.
- **Battle Fest**: Each player starts with their faction's smallest ship. Upon defeat, respawn in the next-larger ship. Last player with ships remaining wins.

---

## 11. Key Design Takeaways for StarRaster Implementation

### What Makes It Fun
1. **Shield facing management** — constantly maneuvering to protect weak shields creates emergent tactical gameplay.
2. **Energy trade-offs** — offense vs. defense is never solved; it shifts moment-to-moment.
3. **Weapon arcs** — positioning matters. You can't just point and shoot.
4. **Ship diversity** — each faction plays differently due to unique abilities and stats.
5. **Crew progression** — RPG elements give long-term investment and playstyle identity.

### What Could Be Improved
1. **Combat variety** — reviewers noted battles felt repetitive after a while. More environmental hazards (asteroids, nebulae, gravity wells) would help.
2. **UI clarity** — weapon/shield status was hard to read. Clear HUD is essential.
3. **AI behavior** — enemy AI was predictable. Varied AI personalities would add depth.
4. **Pacing** — long stretches between encounters. Tighter mission pacing needed.

### What Fits StarRaster Well
- **2D top-down combat** — perfect for our rasterizer's strengths (textured sprites/quads, simple lighting).
- **Palette-indexed rendering** — retro aesthetic matches a TOS-era Star Trek game beautifully.
- **Tile-based spatial systems** — weapon range checks, shield arc calculations, point lights for weapon impacts all map well to our existing spatial grid.
- **Low polygon count** — ship models as simple textured geometry keep us well within performance budgets.

---

## References

- [Star Trek: Tactical Assault — Memory Alpha](https://memory-alpha.fandom.com/wiki/Star_Trek:_Tactical_Assault)
- [Star Trek: Tactical Assault — Wikipedia](https://en.wikipedia.org/wiki/Star_Trek:_Tactical_Assault)
- [Nintendo Life Review](https://www.nintendolife.com/reviews/2010/05/star_trek_tactical_assault_ds)
- [WorthPlaying PSP Preview](https://worthplaying.com/article/2006/11/2/previews/37467-psp-preview-star-trek-tactical-assault/)
- [WorthPlaying NDS/PSP Preview](https://worthplaying.com/article/2006/5/16/previews/33307-ndspsp-preview-star-trek-tactical-assault/)
- [GameFAQs Guide](https://gamefaqs.gamespot.com/ds/931492-star-trek-tactical-assault/faqs/49918)
- [Developer FAQ (TrekCore)](http://gaming.trekcore.com/tacticalassault/tafaq.pdf)
