#ifndef SR_CONFIG_H
#define SR_CONFIG_H

/*  Minimal YAML config loader for StarRaster.
 *
 *  Supports a subset of YAML:
 *    - Nested keys via indentation (2-space)
 *    - Scalar values (float)
 *    - Inline arrays: [1.0, 2.0, 3.0]
 *    - Comments (#)
 *
 *  Usage:
 *    sr_config cfg = sr_config_load("config/dungeon.yaml");
 *    float val = sr_config_float(&cfg, "torch.brightness", 1.0f);
 *    int n = sr_config_array(&cfg, "fog.start", out, max_count);
 *    sr_config_free(&cfg);
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SR_CFG_MAX_ENTRIES 64
#define SR_CFG_MAX_KEY     128
#define SR_CFG_MAX_VAL     256

typedef struct {
    char key[SR_CFG_MAX_KEY];   /* dot-separated path, e.g. "torch.color" */
    char val[SR_CFG_MAX_VAL];   /* raw value string */
} sr_config_entry;

typedef struct {
    sr_config_entry entries[SR_CFG_MAX_ENTRIES];
    int count;
} sr_config;

/* ── Parsing ─────────────────────────────────────────────────────── */

static void sr_config_trim(char *s) {
    /* trim trailing whitespace/newline */
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' ||
                        s[len-1] == ' '  || s[len-1] == '\t'))
        s[--len] = '\0';
}

static int sr_config_indent(const char *line) {
    int n = 0;
    while (line[n] == ' ') n++;
    return n;
}

static sr_config sr_config_load(const char *path) {
    sr_config cfg;
    memset(&cfg, 0, sizeof(cfg));

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[sr_config] Failed to open: %s\n", path);
        return cfg;
    }

    char line[512];
    /* Stack of prefix keys at each indentation level */
    char prefix[4][64];
    int prefix_depth = 0;
    int prev_indent = 0;

    memset(prefix, 0, sizeof(prefix));

    while (fgets(line, sizeof(line), f)) {
        sr_config_trim(line);

        /* Skip empty lines and comments */
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;

        int indent = sr_config_indent(line);

        /* Find key:value split */
        const char *colon = strchr(p, ':');
        if (!colon) continue;

        /* Extract key name */
        char key_part[64];
        int klen = (int)(colon - p);
        if (klen >= 64) klen = 63;
        memcpy(key_part, p, klen);
        key_part[klen] = '\0';
        sr_config_trim(key_part);

        /* Determine depth from indentation (2-space indent) */
        int depth = indent / 2;
        if (depth > 3) depth = 3;

        /* Update prefix stack */
        if (depth <= prefix_depth) {
            prefix_depth = depth;
        }
        strncpy(prefix[depth], key_part, 63);
        prefix[depth][63] = '\0';

        /* Check if there's a value after the colon */
        const char *val_start = colon + 1;
        while (*val_start == ' ' || *val_start == '\t') val_start++;

        /* Strip inline comments */
        char val_buf[SR_CFG_MAX_VAL];
        strncpy(val_buf, val_start, SR_CFG_MAX_VAL - 1);
        val_buf[SR_CFG_MAX_VAL - 1] = '\0';

        /* Don't strip # inside brackets (arrays) */
        if (val_buf[0] != '[') {
            char *comment = strchr(val_buf, '#');
            if (comment) *comment = '\0';
            sr_config_trim(val_buf);
        }

        if (val_buf[0] == '\0') {
            /* No value — this is a parent key, push prefix */
            prefix_depth = depth + 1;
            prev_indent = indent;
            continue;
        }

        /* Build full dotted key path */
        if (cfg.count >= SR_CFG_MAX_ENTRIES) break;

        sr_config_entry *e = &cfg.entries[cfg.count];
        e->key[0] = '\0';
        for (int i = 0; i < depth; i++) {
            strcat(e->key, prefix[i]);
            strcat(e->key, ".");
        }
        strcat(e->key, key_part);
        strncpy(e->val, val_buf, SR_CFG_MAX_VAL - 1);
        e->val[SR_CFG_MAX_VAL - 1] = '\0';
        cfg.count++;

        prefix_depth = depth;
        prev_indent = indent;
    }

    fclose(f);
    return cfg;
}

/* ── Lookup helpers ──────────────────────────────────────────────── */

static const char *sr_config_get(const sr_config *cfg, const char *key) {
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].key, key) == 0)
            return cfg->entries[i].val;
    }
    return NULL;
}

static float sr_config_float(const sr_config *cfg, const char *key, float def) {
    const char *v = sr_config_get(cfg, key);
    if (!v) return def;
    return (float)atof(v);
}

/* Parse an inline YAML array like "[1.0, 2.0, 3.0]" into float array.
 * Returns number of elements parsed. */
static int sr_config_array(const sr_config *cfg, const char *key,
                            float *out, int max_count) {
    const char *v = sr_config_get(cfg, key);
    if (!v) return 0;

    /* Skip opening bracket */
    const char *p = v;
    while (*p && *p != '[') p++;
    if (*p == '[') p++;

    int count = 0;
    while (*p && *p != ']' && count < max_count) {
        while (*p == ' ' || *p == ',') p++;
        if (*p == ']' || *p == '\0') break;
        out[count++] = (float)atof(p);
        /* Skip past number */
        while (*p && *p != ',' && *p != ']') p++;
    }
    return count;
}

static void sr_config_free(sr_config *cfg) {
    cfg->count = 0;
}

/* ── Debug dump ──────────────────────────────────────────────────── */

static void sr_config_dump(const sr_config *cfg) {
    printf("[sr_config] %d entries:\n", cfg->count);
    for (int i = 0; i < cfg->count; i++) {
        printf("  %s = %s\n", cfg->entries[i].key, cfg->entries[i].val);
    }
}

#endif /* SR_CONFIG_H */
