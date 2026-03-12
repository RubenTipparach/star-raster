/*  sr_lighting.h — Directional sun, point lights, shadow map, palette lighting.
 *  Single-TU header-only. Depends on sr_app.h, sr_math.h, sr_raster.h. */
#ifndef SR_LIGHTING_H
#define SR_LIGHTING_H

/* Directional sun light */
#define SUN_X          0.3f
#define SUN_Y          0.8f
#define SUN_Z          0.5f
#define AMBIENT        0.4f
#define DIFFUSE        0.6f

static bool night_mode = false;

#define NIGHT_SKY_COLOR   0xFF3B1A0A   /* dark navy blue (ABGR) */
#define PAL_NIGHT_SKY     0xFF1E0E0A   /* darker sky for palette scene (ABGR) */
static float pal_ambient    = 0.12f;
static float pal_light_mult = 1.0f;
static bool  adjusting_ambient = true;
static bool  pixel_lighting = false;
static bool  shadows_enabled = false;
#define PAL_DIFFUSE        0.0f
#define NIGHT_AMBIENT     0.05f

/* ── Shadow map ──────────────────────────────────────────────────── */

#define SHADOW_SIZE  256
#define SHADOW_BIAS  0.008f

static sr_framebuffer shadow_fb;
static sr_mat4        light_vp;

static float shadow_test(float wx, float wy, float wz) {
    if (!shadows_enabled) return 1.0f;
    sr_vec4 lp = sr_mat4_mul_v4(light_vp, sr_v4(wx, wy, wz, 1.0f));
    if (lp.w <= 0.001f) return 1.0f;
    float lx = lp.x / lp.w, ly = lp.y / lp.w, lz = lp.z / lp.w;
    int sx = (int)((lx + 1.0f) * 0.5f * SHADOW_SIZE);
    int sy = (int)((1.0f - ly) * 0.5f * SHADOW_SIZE);
    if (sx < 0 || sx >= SHADOW_SIZE || sy < 0 || sy >= SHADOW_SIZE) return 1.0f;
    float stored_z = shadow_fb.depth[sy * SHADOW_SIZE + sx];
    return (lz - SHADOW_BIAS > stored_z) ? 0.0f : 1.0f;
}

/* ── Point lights ────────────────────────────────────────────────── */

typedef struct {
    float x, y, z;
    float r, g, b;
    float radius;
} point_light;

#define MAX_LIGHTS 16
static point_light lights[MAX_LIGHTS];
static int         num_lights = 0;

/* ── Spatial grid for fast light lookup ──────────────────────────── */

#define GRID_SIZE    8
#define GRID_ORIGIN -32.0f
#define GRID_EXTENT  64.0f
#define GRID_CELL   (GRID_EXTENT / GRID_SIZE)

static struct {
    uint8_t count;
    uint8_t indices[MAX_LIGHTS];
} light_grid[GRID_SIZE][GRID_SIZE];

static void build_light_grid(void) {
    for (int gz = 0; gz < GRID_SIZE; gz++)
        for (int gx = 0; gx < GRID_SIZE; gx++)
            light_grid[gz][gx].count = 0;

    for (int i = 0; i < num_lights; i++) {
        float r = lights[i].radius;
        int x0 = (int)((lights[i].x - r - GRID_ORIGIN) / GRID_CELL);
        int x1 = (int)((lights[i].x + r - GRID_ORIGIN) / GRID_CELL);
        int z0 = (int)((lights[i].z - r - GRID_ORIGIN) / GRID_CELL);
        int z1 = (int)((lights[i].z + r - GRID_ORIGIN) / GRID_CELL);
        if (x0 < 0) x0 = 0; if (x1 >= GRID_SIZE) x1 = GRID_SIZE - 1;
        if (z0 < 0) z0 = 0; if (z1 >= GRID_SIZE) z1 = GRID_SIZE - 1;
        for (int gz = z0; gz <= z1; gz++) {
            for (int gx = x0; gx <= x1; gx++) {
                uint8_t c = light_grid[gz][gx].count;
                if (c < MAX_LIGHTS) {
                    light_grid[gz][gx].indices[c] = (uint8_t)i;
                    light_grid[gz][gx].count = c + 1;
                }
            }
        }
    }
}

/* Pack brightness into vertex color */
static inline uint32_t brightness_to_color(float r, float g, float b) {
    if (r > 1.0f) r = 1.0f;
    if (g > 1.0f) g = 1.0f;
    if (b > 1.0f) b = 1.0f;
    uint8_t ri = (uint8_t)(r * 255.0f);
    uint8_t gi = (uint8_t)(g * 255.0f);
    uint8_t bi = (uint8_t)(b * 255.0f);
    return 0xFF000000 | ((uint32_t)bi << 16) | ((uint32_t)gi << 8) | ri;
}

/* Compute vertex color from directional sun + point lights */
static uint32_t vertex_light(float px, float py, float pz,
                             float nx, float ny, float nz)
{
    float amb = night_mode ? NIGHT_AMBIENT : AMBIENT;
    float lr = amb, lg = amb, lb = amb;

    if (!night_mode) {
        float len = sqrtf(SUN_X*SUN_X + SUN_Y*SUN_Y + SUN_Z*SUN_Z);
        float sx = SUN_X/len, sy = SUN_Y/len, sz = SUN_Z/len;
        float dot = nx*sx + ny*sy + nz*sz;
        if (dot < 0.0f) dot = 0.0f;
        lr += DIFFUSE * dot;
        lg += DIFFUSE * dot;
        lb += DIFFUSE * dot;
    }

    int gx = (int)((px - GRID_ORIGIN) / GRID_CELL);
    int gz = (int)((pz - GRID_ORIGIN) / GRID_CELL);
    if (gx >= 0 && gx < GRID_SIZE && gz >= 0 && gz < GRID_SIZE) {
        int n = light_grid[gz][gx].count;
        for (int ii = 0; ii < n; ii++) {
            int i = light_grid[gz][gx].indices[ii];
            float dx = lights[i].x - px;
            float dy = lights[i].y - py;
            float dz = lights[i].z - pz;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist >= lights[i].radius) continue;
            float atten = 1.0f - dist / lights[i].radius;
            atten *= atten;
            float inv_dist = 1.0f / (dist + 0.001f);
            float dot = (dx * nx + dy * ny + dz * nz) * inv_dist;
            if (dot < 0.0f) dot = 0.0f;
            lr += lights[i].r * atten * dot;
            lg += lights[i].g * atten * dot;
            lb += lights[i].b * atten * dot;
        }
    }

    return brightness_to_color(lr, lg, lb);
}

static uint32_t face_tint(float nx, float ny, float nz) {
    return vertex_light(0, 0, 0, nx, ny, nz);
}

static uint32_t face_tint_rotY(float lnx, float lny, float lnz, float rot_y) {
    float c = cosf(rot_y), s = sinf(rot_y);
    float wx = lnx * c + lnz * s;
    float wy = lny;
    float wz = -lnx * s + lnz * c;
    return face_tint(wx, wy, wz);
}

static uint32_t vertex_light_rotY(float wpx, float wpy, float wpz,
                                  float lnx, float lny, float lnz, float rot_y) {
    float c = cosf(rot_y), s = sinf(rot_y);
    return vertex_light(wpx, wpy, wpz,
                        lnx*c + lnz*s, lny, -lnx*s + lnz*c);
}

/* ── Palette lighting ────────────────────────────────────────────── */

static inline uint32_t pal_intensity_color(float intensity) {
    int val = (int)(intensity * 128.0f);
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    return 0xFF000000 | (uint32_t)val | ((uint32_t)val << 8) | ((uint32_t)val << 16);
}

static float pal_vertex_intensity(float px, float py, float pz,
                                   float nx, float ny, float nz)
{
    float total = pal_ambient;
    float shadow = shadow_test(px, py, pz);

    int gx = (int)((px - GRID_ORIGIN) / GRID_CELL);
    int gz = (int)((pz - GRID_ORIGIN) / GRID_CELL);
    if (gx >= 0 && gx < GRID_SIZE && gz >= 0 && gz < GRID_SIZE) {
        int n = light_grid[gz][gx].count;
        for (int ii = 0; ii < n; ii++) {
            int i = light_grid[gz][gx].indices[ii];
            float dx = lights[i].x - px;
            float dy = lights[i].y - py;
            float dz = lights[i].z - pz;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist >= lights[i].radius) continue;
            float atten = 1.0f - dist / lights[i].radius;
            atten *= atten;
            float inv_dist = 1.0f / (dist + 0.001f);
            float ldot = (dx * nx + dy * ny + dz * nz) * inv_dist;
            if (ldot < 0.0f) ldot = 0.0f;
            float lum = (lights[i].r + lights[i].g + lights[i].b) * (1.0f / 3.0f);
            total += lum * atten * ldot * shadow;
        }
    }

    return total;
}

#endif /* SR_LIGHTING_H */
