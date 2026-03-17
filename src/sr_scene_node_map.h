/*  sr_scene_node_map.h — FTL-style sector map with node-based navigation.
 *  Single-TU header-only. Depends on sr_app.h, sr_raster.h, sr_font.h.
 *  Must be included AFTER sr_scene_space_fleet.h (references sfa). */
#ifndef SR_SCENE_NODE_MAP_H
#define SR_SCENE_NODE_MAP_H

#include <math.h>
#include <stdbool.h>
#include <string.h>

/* ── Constants ───────────────────────────────────────────────────── */

#define NM_MAX_NODES      40
#define NM_MAX_EDGES      96
#define NM_MAP_COLS        8

/* Node types */
enum {
    NM_NODE_START,
    NM_NODE_COMBAT,
    NM_NODE_EVENT,
    NM_NODE_SHOP,
    NM_NODE_BOSS,
    NM_NODE_EXIT
};

/* Event sub-types */
enum { NM_EVT_DISTRESS, NM_EVT_TRADER, NM_EVT_ANOMALY };

/* Colors (ABGR) */
#define NM_BG_COLOR        0xFF0E0808
#define NM_GRID_COLOR      0xFF1A1212
#define NM_NODE_COMBAT_COL 0xFF3344DD
#define NM_NODE_EVENT_COL  0xFFDDCC44
#define NM_NODE_SHOP_COL   0xFF44DD44
#define NM_NODE_BOSS_COL   0xFF2222FF
#define NM_NODE_START_COL  0xFFCCCCCC
#define NM_NODE_EXIT_COL   0xFFCCCCCC
#define NM_EDGE_COLOR      0xFF333333
#define NM_EDGE_ACTIVE_COL 0xFF666655
#define NM_PLAYER_COL      0xFF55CCFF
#define NM_TEXT_COL        0xFFCCCCCC
#define NM_TEXT_BRIGHT     0xFFFFFFFF
#define NM_TEXT_DIM        0xFF888888
#define NM_TEXT_SHADOW     0xFF000000
#define NM_HUD_BG         0xC0000000
#define NM_HIGHLIGHT_COL   0xFF88EEFF

/* ── Data structures ─────────────────────────────────────────────── */

typedef struct {
    int   type;
    int   sub_type;
    int   x, y;
    bool  visited;
    bool  reachable;
    int   enemy_count;
    int   enemy_classes[4];
    int   reward;
    const char *event_title;
    const char *event_desc;
    int   event_choice_credits;
} nm_node;

typedef struct { int a, b; } nm_edge;

typedef struct {
    bool   active;
    int    node_idx;
    const char *title;
    const char *desc;
    const char *choice_a;
    const char *choice_b;
    bool   show_result;
    const char *result_text;
    int    credits_delta;
} nm_dialog;

typedef struct {
    nm_node   nodes[NM_MAX_NODES];
    int       node_count;
    nm_edge   edges[NM_MAX_EDGES];
    int       edge_count;
    int       current_node;
    int       hovered_node;
    float     time;
    bool      initialized;
    nm_dialog dialog;
    bool      shop_open;
    int       shop_node;
    int       shop_hover;
    float     mouse_fb_x, mouse_fb_y;
} nm_state;

static nm_state nm;

/* ── RNG ─────────────────────────────────────────────────────────── */

static uint32_t nm_rng = 0;
static float nm_randf(void) {
    nm_rng = nm_rng * 1103515245u + 12345u;
    return (float)((nm_rng >> 16) & 0x7FFF) / 32768.0f;
}
static int nm_randi(int lo, int hi) {
    return lo + (int)(nm_randf() * (float)(hi - lo + 1));
}

/* ── Forward declarations ────────────────────────────────────────── */
static void nm_update_reachable(void);

/* ── Map generation ──────────────────────────────────────────────── */

/* Compute enemy setup for a combat node based on column depth (0-7).
   Early columns: small ships (frigates). Later: bigger ships. */
static void nm_setup_combat_for_depth(nm_node *n, int depth, int sector) {
    /* depth 1-2: 1 frigate, depth 3-4: 1-2 frigates/destroyers,
       depth 5-6: 2-3 destroyers/cruisers */
    if (depth <= 2) {
        n->enemy_count = 1;
        for (int e = 0; e < n->enemy_count; e++)
            n->enemy_classes[e] = SHIP_CLASS_FRIGATE;
    } else if (depth <= 4) {
        n->enemy_count = 1 + (int)(nm_randf() * 1.5f);
        if (n->enemy_count > 2) n->enemy_count = 2;
        int max_cls = SHIP_CLASS_DESTROYER;
        for (int e = 0; e < n->enemy_count; e++)
            n->enemy_classes[e] = nm_randi(SHIP_CLASS_FRIGATE, max_cls);
    } else {
        n->enemy_count = 2 + (int)(nm_randf() * 1.5f);
        if (n->enemy_count > 3) n->enemy_count = 3;
        int max_cls = SHIP_CLASS_CRUISER;
        for (int e = 0; e < n->enemy_count; e++)
            n->enemy_classes[e] = nm_randi(SHIP_CLASS_DESTROYER, max_cls);
    }
    n->reward = 80 + depth * 30 + sector * 40 + n->enemy_count * 30;
}

static void nm_generate_sector(int sector) {
    memset(&nm, 0, sizeof(nm));
    nm.hovered_node = -1;
    nm.shop_hover = -1;

    nm_rng = 54321u + (uint32_t)sector * 77777u;

    int cols = NM_MAP_COLS;
    /* 8 evenly-spaced columns across the framebuffer */
    int col_x[NM_MAP_COLS];
    for (int c = 0; c < cols; c++)
        col_x[c] = 30 + c * (FB_WIDTH - 60) / (cols - 1);

    int col_start[NM_MAP_COLS + 1];
    int idx = 0;

    /* Start node (column 0) */
    nm.nodes[idx].type = NM_NODE_START;
    nm.nodes[idx].x = col_x[0];
    nm.nodes[idx].y = FB_HEIGHT / 2;
    nm.nodes[idx].visited = true;
    col_start[0] = 0;
    idx++;

    /* Middle columns (1 through cols-2): 3-4 random nodes each */
    for (int c = 1; c < cols - 1; c++) {
        col_start[c] = idx;
        int count = 3 + (int)(nm_randf() * 1.99f); /* 3 or 4 */
        if (count > 4) count = 4;
        int spacing = FB_HEIGHT / (count + 1);

        for (int r = 0; r < count; r++) {
            nm_node *n = &nm.nodes[idx];
            n->x = col_x[c] + nm_randi(-8, 8);
            n->y = spacing * (r + 1) + nm_randi(-6, 6);
            if (n->y < 20) n->y = 20;
            if (n->y > FB_HEIGHT - 20) n->y = FB_HEIGHT - 20;

            float roll = nm_randf();
            if (roll < 0.55f) {
                n->type = NM_NODE_COMBAT;
                nm_setup_combat_for_depth(n, c, sector);
            } else if (roll < 0.80f) {
                n->type = NM_NODE_EVENT;
                n->sub_type = nm_randi(0, 2);
                switch (n->sub_type) {
                    case NM_EVT_DISTRESS:
                        n->event_title = "DISTRESS SIGNAL";
                        n->event_desc = "A civilian freighter is under attack.";
                        n->reward = 80 + c * 15 + sector * 30;
                        n->event_choice_credits = 80 + c * 15 + sector * 30;
                        break;
                    case NM_EVT_TRADER:
                        n->event_title = "TRADER VESSEL";
                        n->event_desc = "A Ferengi merchant hails your ship.";
                        n->event_choice_credits = 50 + c * 10 + sector * 20;
                        break;
                    case NM_EVT_ANOMALY:
                        n->event_title = "SPACE ANOMALY";
                        n->event_desc = "Sensors detect unusual energy readings.";
                        n->event_choice_credits = 60 + c * 12 + sector * 25;
                        break;
                }
            } else {
                n->type = NM_NODE_SHOP;
            }
            idx++;
        }
    }

    /* Ensure at least 2 shops exist in the middle columns */
    {
        int shop_count = 0;
        for (int i = col_start[1]; i < idx; i++) {
            if (nm.nodes[i].type == NM_NODE_SHOP) shop_count++;
        }
        while (shop_count < 2 && idx > col_start[1]) {
            /* Pick a random middle node that isn't already a shop */
            int pick = col_start[1] + nm_randi(0, idx - col_start[1] - 1);
            if (nm.nodes[pick].type != NM_NODE_SHOP) {
                nm.nodes[pick].type = NM_NODE_SHOP;
                shop_count++;
            }
        }
    }

    /* Boss node (final column) — Klingon Battlecruiser */
    col_start[cols - 1] = idx;
    nm.nodes[idx].type = NM_NODE_BOSS;
    nm.nodes[idx].x = col_x[cols - 1];
    nm.nodes[idx].y = FB_HEIGHT / 2;
    nm.nodes[idx].enemy_count = 1 + sector;
    if (nm.nodes[idx].enemy_count > 3) nm.nodes[idx].enemy_count = 3;
    /* Lead enemy is always a Battlecruiser; escorts scale with sector */
    nm.nodes[idx].enemy_classes[0] = SHIP_CLASS_BATTLECRUISER;
    for (int e = 1; e < nm.nodes[idx].enemy_count; e++)
        nm.nodes[idx].enemy_classes[e] = nm_randi(SHIP_CLASS_CRUISER, SHIP_CLASS_BATTLECRUISER);
    nm.nodes[idx].reward = 300 + sector * 100;
    idx++;
    col_start[cols] = idx;
    nm.node_count = idx;

    /* Generate edges */
    nm.edge_count = 0;
    for (int c = 0; c < cols - 1; c++) {
        int from_start = col_start[c];
        int from_end   = col_start[c + 1];
        int to_start   = col_start[c + 1];
        int to_end     = col_start[c + 2];

        for (int f = from_start; f < from_end; f++) {
            int best = to_start;
            int best_dy = 9999;
            for (int t = to_start; t < to_end; t++) {
                int dy = abs(nm.nodes[f].y - nm.nodes[t].y);
                if (dy < best_dy) { best_dy = dy; best = t; }
            }
            nm.edges[nm.edge_count].a = f;
            nm.edges[nm.edge_count].b = best;
            nm.edge_count++;

            if (nm_randf() < 0.5f && to_end - to_start > 1) {
                int second = to_start + nm_randi(0, to_end - to_start - 1);
                if (second != best) {
                    nm.edges[nm.edge_count].a = f;
                    nm.edges[nm.edge_count].b = second;
                    nm.edge_count++;
                }
            }
        }

        /* Ensure every next-column node has at least one incoming edge */
        for (int t = to_start; t < to_end; t++) {
            bool has_edge = false;
            for (int e = 0; e < nm.edge_count; e++)
                if (nm.edges[e].b == t) { has_edge = true; break; }
            if (!has_edge) {
                int best = from_start;
                int best_dy = 9999;
                for (int f = from_start; f < from_end; f++) {
                    int dy = abs(nm.nodes[f].y - nm.nodes[t].y);
                    if (dy < best_dy) { best_dy = dy; best = f; }
                }
                nm.edges[nm.edge_count].a = best;
                nm.edges[nm.edge_count].b = t;
                nm.edge_count++;
            }
        }
    }

    nm.current_node = 0;
    nm_update_reachable();
}

/* ── Reachability ────────────────────────────────────────────────── */

static void nm_update_reachable(void) {
    for (int i = 0; i < nm.node_count; i++)
        nm.nodes[i].reachable = false;
    for (int e = 0; e < nm.edge_count; e++) {
        if (nm.edges[e].a == nm.current_node)
            nm.nodes[nm.edges[e].b].reachable = true;
        if (nm.edges[e].b == nm.current_node)
            nm.nodes[nm.edges[e].a].reachable = true;
    }
}

/* ── Init ────────────────────────────────────────────────────────── */

static void nm_init(void) {
    if (!campaign.campaign_active) {
        campaign.campaign_active = true;
        campaign.credits = 200;
        campaign.player_ship_class = SHIP_CLASS_FRIGATE;
        campaign.sector = 0;
        campaign.current_node = 0;
        campaign.combat_victory = false;
    }

    nm_generate_sector(campaign.sector);

    if (campaign.combat_victory) {
        nm.nodes[campaign.current_node].visited = true;
        nm.current_node = campaign.current_node;
        campaign.credits += campaign.encounter_reward;
        campaign.combat_victory = false;
        nm_update_reachable();
    } else {
        nm.current_node = campaign.current_node;
        nm_update_reachable();
    }

    nm.initialized = true;
}

/* ── Drawing helpers ─────────────────────────────────────────────── */

static void nm_draw_rect(uint32_t *px, int W, int H,
                          int x0, int y0, int x1, int y1, uint32_t col) {
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    for (int ry = y0; ry < y1; ry++)
        for (int rx = x0; rx < x1; rx++)
            px[ry * W + rx] = col;
}

static void nm_draw_circle(uint32_t *px, int W, int H,
                             int cx, int cy, int r, uint32_t col) {
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy > r2) continue;
            int px2 = cx + dx, py2 = cy + dy;
            if (px2 >= 0 && px2 < W && py2 >= 0 && py2 < H)
                px[py2 * W + px2] = col;
        }
}

static void nm_draw_ring(uint32_t *px, int W, int H,
                           int cx, int cy, int r, int thickness, uint32_t col) {
    int outer2 = (r + thickness) * (r + thickness);
    int inner2 = r * r;
    for (int dy = -(r+thickness); dy <= r+thickness; dy++)
        for (int dx = -(r+thickness); dx <= r+thickness; dx++) {
            int d2 = dx*dx + dy*dy;
            if (d2 > outer2 || d2 < inner2) continue;
            int px2 = cx + dx, py2 = cy + dy;
            if (px2 >= 0 && px2 < W && py2 >= 0 && py2 < H)
                px[py2 * W + px2] = col;
        }
}

static void nm_draw_line(uint32_t *px, int W, int H,
                           int x0, int y0, int x1, int y1, uint32_t col) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < W && y0 >= 0 && y0 < H)
            px[y0 * W + x0] = col;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static uint32_t nm_node_color(int type) {
    switch (type) {
        case NM_NODE_COMBAT: return NM_NODE_COMBAT_COL;
        case NM_NODE_EVENT:  return NM_NODE_EVENT_COL;
        case NM_NODE_SHOP:   return NM_NODE_SHOP_COL;
        case NM_NODE_BOSS:   return NM_NODE_BOSS_COL;
        case NM_NODE_START:  return NM_NODE_START_COL;
        default:             return NM_TEXT_DIM;
    }
}

static const char *nm_node_label(int type) {
    switch (type) {
        case NM_NODE_COMBAT: return "!";
        case NM_NODE_EVENT:  return "?";
        case NM_NODE_SHOP:   return "$";
        case NM_NODE_BOSS:   return "X";
        case NM_NODE_START:  return "S";
        default:             return ".";
    }
}

/* ── Draw the sector map ─────────────────────────────────────────── */

static void nm_draw_map(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    /* Title bar */
    nm_draw_rect(px, W, H, 0, 0, W, 14, NM_HUD_BG);
    char title[64];
    snprintf(title, sizeof(title), "SECTOR %d  -  %d CREDITS", campaign.sector + 1, campaign.credits);
    sr_draw_text_shadow(px, W, H, 4, 3, title, NM_TEXT_BRIGHT, NM_TEXT_SHADOW);

    char ship_info[48];
    snprintf(ship_info, sizeof(ship_info), "SHIP: %s", ship_class_names[campaign.player_ship_class]);
    sr_draw_text_shadow(px, W, H, W - 6 * (int)strlen(ship_info) - 4, 3, ship_info, NM_HIGHLIGHT_COL, NM_TEXT_SHADOW);

    /* Draw edges */
    for (int e = 0; e < nm.edge_count; e++) {
        nm_node *a = &nm.nodes[nm.edges[e].a];
        nm_node *b = &nm.nodes[nm.edges[e].b];
        bool is_reachable = (nm.edges[e].a == nm.current_node && b->reachable) ||
                            (nm.edges[e].b == nm.current_node && a->reachable);
        uint32_t ecol = (a->visited && b->visited) ? NM_EDGE_ACTIVE_COL :
                        is_reachable ? NM_EDGE_ACTIVE_COL : NM_EDGE_COLOR;
        nm_draw_line(px, W, H, a->x, a->y, b->x, b->y, ecol);
    }

    /* Draw nodes */
    for (int i = 0; i < nm.node_count; i++) {
        nm_node *n = &nm.nodes[i];
        int r = 6;
        uint32_t col = nm_node_color(n->type);

        if (n->visited && i != nm.current_node) col = NM_TEXT_DIM;

        if (n->reachable && !n->visited)
            nm_draw_ring(px, W, H, n->x, n->y, r + 2, 1, NM_HIGHLIGHT_COL);

        if (i == nm.hovered_node && n->reachable)
            nm_draw_circle(px, W, H, n->x, n->y, r + 3, NM_HIGHLIGHT_COL);

        nm_draw_circle(px, W, H, n->x, n->y, r, col);

        if (i == nm.current_node)
            nm_draw_ring(px, W, H, n->x, n->y, r + 2, 2, NM_PLAYER_COL);

        sr_draw_text_shadow(px, W, H, n->x - 2, n->y - 3, nm_node_label(n->type), NM_TEXT_BRIGHT, NM_TEXT_SHADOW);

        /* Tooltip on hover */
        if (i == nm.hovered_node && n->reachable) {
            const char *type_name = "";
            switch (n->type) {
                case NM_NODE_COMBAT: type_name = "COMBAT"; break;
                case NM_NODE_EVENT:  type_name = n->event_title ? n->event_title : "EVENT"; break;
                case NM_NODE_SHOP:   type_name = "SHIPYARD"; break;
                case NM_NODE_BOSS:   type_name = "BOSS BATTLE"; break;
                default: break;
            }
            if (type_name[0]) {
                int tx = n->x - (int)strlen(type_name) * 3;
                sr_draw_text_shadow(px, W, H, tx, n->y - 16, type_name, NM_TEXT_BRIGHT, NM_TEXT_SHADOW);
            }
            if (n->type == NM_NODE_COMBAT || n->type == NM_NODE_BOSS) {
                char rbuf[32];
                snprintf(rbuf, sizeof(rbuf), "%d CR / %d ENEMY", n->reward, n->enemy_count);
                int tx = n->x - (int)strlen(rbuf) * 3;
                sr_draw_text_shadow(px, W, H, tx, n->y + 12, rbuf, NM_TEXT_COL, NM_TEXT_SHADOW);
            }
        }
    }

    sr_draw_text_shadow(px, W, H, 4, H - 12, "TAP A NODE TO TRAVEL", NM_TEXT_DIM, NM_TEXT_SHADOW);
}

/* ── Shop overlay ────────────────────────────────────────────────── */

static void nm_draw_shop(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    for (int i = 0; i < W * H; i++) {
        uint32_t c = px[i];
        uint8_t r = ((c) & 0xFF) >> 1;
        uint8_t g = ((c >> 8) & 0xFF) >> 1;
        uint8_t b = ((c >> 16) & 0xFF) >> 1;
        px[i] = 0xFF000000 | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    }

    int px0 = 60, py0 = 30, px1 = W - 60, py1 = H - 30;
    nm_draw_rect(px, W, H, px0, py0, px1, py1, 0xE0111111);
    nm_draw_rect(px, W, H, px0, py0, px1, py0 + 14, 0xE0222244);

    sr_draw_text_shadow(px, W, H, px0 + 4, py0 + 3, "SHIPYARD", NM_TEXT_BRIGHT, NM_TEXT_SHADOW);

    char cred[32];
    snprintf(cred, sizeof(cred), "CREDITS: %d", campaign.credits);
    sr_draw_text_shadow(px, W, H, px1 - 6 * (int)strlen(cred) - 4, py0 + 3, cred, NM_HIGHLIGHT_COL, NM_TEXT_SHADOW);

    int row_y = py0 + 20;
    for (int i = 0; i < SHIP_CLASS_COUNT; i++) {
        const ship_class_stats *sc = &ship_classes[i];
        bool owned = (campaign.player_ship_class == i);
        bool can_afford = (campaign.credits >= sc->cost);
        bool hovered = (nm.shop_hover == i);

        uint32_t bg = hovered ? 0xE0333344 : 0xE0181818;
        nm_draw_rect(px, W, H, px0 + 4, row_y, px1 - 4, row_y + 30, bg);

        uint32_t name_col = owned ? NM_HIGHLIGHT_COL : (can_afford ? NM_TEXT_BRIGHT : NM_TEXT_DIM);
        sr_draw_text_shadow(px, W, H, px0 + 8, row_y + 2, ship_class_names[i], name_col, NM_TEXT_SHADOW);

        char stats[64];
        snprintf(stats, sizeof(stats), "HP:%d SH:%.0f SPD:%.1fx TRN:%.1fx",
                 sc->hull_max, sc->shield_max, sc->speed_mult, sc->turn_mult);
        sr_draw_text_shadow(px, W, H, px0 + 8, row_y + 12, stats, NM_TEXT_DIM, NM_TEXT_SHADOW);

        if (owned) {
            sr_draw_text_shadow(px, W, H, px1 - 60, row_y + 2, "EQUIPPED", NM_HIGHLIGHT_COL, NM_TEXT_SHADOW);
        } else if (sc->cost == 0) {
            sr_draw_text_shadow(px, W, H, px1 - 40, row_y + 2, "FREE", NM_NODE_SHOP_COL, NM_TEXT_SHADOW);
        } else {
            char cost_str[16];
            snprintf(cost_str, sizeof(cost_str), "%d CR", sc->cost);
            uint32_t cost_col = can_afford ? NM_NODE_SHOP_COL : NM_NODE_COMBAT_COL;
            sr_draw_text_shadow(px, W, H, px1 - 50, row_y + 2, cost_str, cost_col, NM_TEXT_SHADOW);
        }
        row_y += 34;
    }

    int close_x = px0 + (px1 - px0) / 2 - 20;
    int close_y = py1 - 18;
    nm_draw_rect(px, W, H, close_x, close_y, close_x + 40, close_y + 14, 0xE0333355);
    sr_draw_text_shadow(px, W, H, close_x + 8, close_y + 3, "CLOSE", NM_TEXT_BRIGHT, NM_TEXT_SHADOW);
}

/* ── Dialog overlay ──────────────────────────────────────────────── */

static void nm_draw_dialog(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    for (int i = 0; i < W * H; i++) {
        uint32_t c = px[i];
        uint8_t r = ((c) & 0xFF) >> 1;
        uint8_t g = ((c >> 8) & 0xFF) >> 1;
        uint8_t b = ((c >> 16) & 0xFF) >> 1;
        px[i] = 0xFF000000 | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    }

    nm_dialog *d = &nm.dialog;
    int px0 = 70, py0 = 40, px1 = W - 70, py1 = H - 40;
    nm_draw_rect(px, W, H, px0, py0, px1, py1, 0xE0111111);
    nm_draw_rect(px, W, H, px0, py0, px1, py0 + 14, 0xE0442222);

    sr_draw_text_shadow(px, W, H, px0 + 4, py0 + 3, d->title, NM_TEXT_BRIGHT, NM_TEXT_SHADOW);

    int ty = py0 + 22;
    if (d->desc)
        sr_draw_text_shadow(px, W, H, px0 + 8, ty, d->desc, NM_TEXT_COL, NM_TEXT_SHADOW);
    ty += 12;

    if (d->show_result) {
        sr_draw_text_shadow(px, W, H, px0 + 8, ty + 8, d->result_text, NM_HIGHLIGHT_COL, NM_TEXT_SHADOW);
        if (d->credits_delta != 0) {
            char cbuf[32];
            snprintf(cbuf, sizeof(cbuf), "%+d CREDITS", d->credits_delta);
            uint32_t ccol = d->credits_delta > 0 ? NM_NODE_SHOP_COL : NM_NODE_COMBAT_COL;
            sr_draw_text_shadow(px, W, H, px0 + 8, ty + 22, cbuf, ccol, NM_TEXT_SHADOW);
        }
        int bx = px0 + (px1 - px0) / 2 - 28;
        int by = py1 - 18;
        nm_draw_rect(px, W, H, bx, by, bx + 56, by + 14, 0xE0333355);
        sr_draw_text_shadow(px, W, H, bx + 6, by + 3, "CONTINUE", NM_TEXT_BRIGHT, NM_TEXT_SHADOW);
    } else {
        int bw = 100;
        int bx1 = px0 + 10;
        int bx2 = px1 - bw - 10;
        int by = py1 - 22;
        nm_draw_rect(px, W, H, bx1, by, bx1 + bw, by + 14, 0xE0334433);
        sr_draw_text_shadow(px, W, H, bx1 + 4, by + 3, d->choice_a, NM_NODE_SHOP_COL, NM_TEXT_SHADOW);
        nm_draw_rect(px, W, H, bx2, by, bx2 + bw, by + 14, 0xE0443333);
        sr_draw_text_shadow(px, W, H, bx2 + 4, by + 3, d->choice_b, NM_NODE_COMBAT_COL, NM_TEXT_SHADOW);
    }
}

/* ── Event helpers ───────────────────────────────────────────────── */

static void nm_open_event_dialog(int node_idx) {
    nm_node *n = &nm.nodes[node_idx];
    nm_dialog *d = &nm.dialog;
    memset(d, 0, sizeof(*d));
    d->active = true;
    d->node_idx = node_idx;
    d->title = n->event_title ? n->event_title : "EVENT";
    d->desc = n->event_desc;

    switch (n->sub_type) {
        case NM_EVT_DISTRESS:
            d->choice_a = "ASSIST (+CR)";
            d->choice_b = "IGNORE (SAFE)";
            break;
        case NM_EVT_TRADER:
            d->choice_a = "TRADE (+CR)";
            d->choice_b = "DECLINE";
            break;
        case NM_EVT_ANOMALY:
            d->choice_a = "INVESTIGATE";
            d->choice_b = "AVOID";
            break;
    }
}

static void nm_resolve_event(int choice) {
    nm_dialog *d = &nm.dialog;
    nm_node *n = &nm.nodes[d->node_idx];
    d->show_result = true;

    if (choice == 1) {
        float luck = nm_randf();
        if (luck > 0.3f) {
            d->credits_delta = n->event_choice_credits;
            switch (n->sub_type) {
                case NM_EVT_DISTRESS: d->result_text = "Rescued the crew!"; break;
                case NM_EVT_TRADER:   d->result_text = "Good trade!"; break;
                case NM_EVT_ANOMALY:  d->result_text = "Found resources!"; break;
            }
        } else {
            d->credits_delta = -(n->event_choice_credits / 2);
            switch (n->sub_type) {
                case NM_EVT_DISTRESS: d->result_text = "It was a trap!"; break;
                case NM_EVT_TRADER:   d->result_text = "Got swindled..."; break;
                case NM_EVT_ANOMALY:  d->result_text = "Shields damaged!"; break;
            }
        }
        campaign.credits += d->credits_delta;
        if (campaign.credits < 0) campaign.credits = 0;
    } else {
        d->credits_delta = 0;
        d->result_text = "Moved on safely.";
    }
}

/* ── Enter a node ────────────────────────────────────────────────── */

static void nm_advance_sector(void) {
    campaign.sector++;
    campaign.current_node = 0;
    nm.initialized = false;
}

static void nm_enter_node(int node_idx) {
    nm_node *n = &nm.nodes[node_idx];
    n->visited = true;
    nm.current_node = node_idx;
    campaign.current_node = node_idx;
    nm_update_reachable();

    switch (n->type) {
        case NM_NODE_COMBAT:
        case NM_NODE_BOSS:
            campaign.encounter_enemy_count = n->enemy_count;
            for (int i = 0; i < n->enemy_count; i++)
                campaign.encounter_enemy_classes[i] = n->enemy_classes[i];
            campaign.encounter_reward = n->reward;
            campaign.combat_victory = false;
            current_scene = SCENE_SPACE_FLEET;
            sfa.initialized = false;
            break;
        case NM_NODE_EVENT:
            nm_open_event_dialog(node_idx);
            break;
        case NM_NODE_SHOP:
            nm.shop_open = true;
            nm.shop_node = node_idx;
            break;
        default:
            break;
    }
}

/* ── Hover detection ─────────────────────────────────────────────── */

static void nm_update_hover(void) {
    nm.hovered_node = -1;
    float best_d = 15.0f;
    for (int i = 0; i < nm.node_count; i++) {
        float dx = nm.mouse_fb_x - (float)nm.nodes[i].x;
        float dy = nm.mouse_fb_y - (float)nm.nodes[i].y;
        float d = sqrtf(dx*dx + dy*dy);
        if (d < best_d) { best_d = d; nm.hovered_node = i; }
    }

    if (nm.shop_open) {
        nm.shop_hover = -1;
        int px0 = 60;
        int row_y = 30 + 20;
        for (int i = 0; i < SHIP_CLASS_COUNT; i++) {
            if (nm.mouse_fb_x >= px0 + 4 && nm.mouse_fb_x <= FB_WIDTH - 60 - 4 &&
                nm.mouse_fb_y >= row_y && nm.mouse_fb_y < row_y + 30)
                nm.shop_hover = i;
            row_y += 34;
        }
    }
}

/* ── Input ───────────────────────────────────────────────────────── */

static void nm_handle_mouse_move(float sx, float sy) {
    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);
    nm.mouse_fb_x = fx;
    nm.mouse_fb_y = fy;
    nm_update_hover();
}

static bool nm_handle_click(float sx, float sy) {
    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);
    nm.mouse_fb_x = fx;
    nm.mouse_fb_y = fy;
    nm_update_hover();

    /* Menu button */
    int mbx = FB_WIDTH - 32, mby = 3;
    if (fx >= mbx && fx <= mbx + 30 && fy >= mby && fy <= mby + 11) {
        app_state = STATE_MENU;
        return true;
    }

    /* Dialog */
    if (nm.dialog.active) {
        nm_dialog *d = &nm.dialog;
        int px0 = 70, px1_d = FB_WIDTH - 70, py1 = FB_HEIGHT - 40;

        if (d->show_result) {
            int bx = px0 + (px1_d - px0) / 2 - 28;
            int by = py1 - 18;
            if (fx >= bx && fx <= bx + 56 && fy >= by && fy <= by + 14) {
                d->active = false;
                if (nm.nodes[nm.current_node].type == NM_NODE_BOSS)
                    nm_advance_sector();
                return true;
            }
        } else {
            int bw = 100;
            int bx1 = px0 + 10;
            int bx2 = px1_d - bw - 10;
            int by = py1 - 22;
            if (fx >= bx1 && fx <= bx1 + bw && fy >= by && fy <= by + 14) {
                nm_resolve_event(1);
                return true;
            }
            if (fx >= bx2 && fx <= bx2 + bw && fy >= by && fy <= by + 14) {
                nm_resolve_event(2);
                return true;
            }
        }
        return true;
    }

    /* Shop */
    if (nm.shop_open) {
        int px0 = 60, px1 = FB_WIDTH - 60, py1 = FB_HEIGHT - 30;
        int close_x = px0 + (px1 - px0) / 2 - 20;
        int close_y = py1 - 18;
        if (fx >= close_x && fx <= close_x + 40 && fy >= close_y && fy <= close_y + 14) {
            nm.shop_open = false;
            return true;
        }
        if (nm.shop_hover >= 0 && nm.shop_hover < SHIP_CLASS_COUNT) {
            int cls = nm.shop_hover;
            if (cls != campaign.player_ship_class && campaign.credits >= ship_classes[cls].cost) {
                campaign.credits -= ship_classes[cls].cost;
                campaign.player_ship_class = cls;
            }
            return true;
        }
        return true;
    }

    /* Node click */
    if (nm.hovered_node >= 0) {
        nm_node *n = &nm.nodes[nm.hovered_node];
        if (n->reachable && !n->visited) {
            nm_enter_node(nm.hovered_node);
            return true;
        }
    }
    return false;
}

/* ── Main scene draw ─────────────────────────────────────────────── */

static void draw_node_map_scene(sr_framebuffer *fb_ptr, float dt) {
    /* Check if returning from combat */
    if (campaign.event_type == -1) {
        campaign.event_type = 0;
        nm.initialized = false;
    }
    if (!nm.initialized) nm_init();
    nm.time += dt;

    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    /* Grid background */
    for (int y = 0; y < H; y += 30)
        for (int x = 0; x < W; x++)
            px[y * W + x] = NM_GRID_COLOR;
    for (int x = 0; x < W; x += 40)
        for (int y = 0; y < H; y++)
            px[y * W + x] = NM_GRID_COLOR;

    /* Dim stars */
    uint32_t star_rng = 99999;
    for (int i = 0; i < 60; i++) {
        star_rng = star_rng * 1103515245u + 12345u;
        int sx = (int)((star_rng >> 16) & 0x1FF) % W;
        star_rng = star_rng * 1103515245u + 12345u;
        int sy = (int)((star_rng >> 16) & 0x1FF) % H;
        if (sx >= 0 && sx < W && sy >= 0 && sy < H)
            px[sy * W + sx] = 0xFF222222 | (star_rng & 0x00333333);
    }

    nm_draw_map(fb_ptr);

    if (nm.shop_open) nm_draw_shop(fb_ptr);
    if (nm.dialog.active) nm_draw_dialog(fb_ptr);

    /* MENU button */
    nm_draw_rect(px, W, H, W - 32, 3, W - 2, 14, 0xC0222222);
    sr_draw_text_shadow(px, W, H, W - 30, 4, "MENU", NM_TEXT_COL, NM_TEXT_SHADOW);
}

#endif /* SR_SCENE_NODE_MAP_H */
