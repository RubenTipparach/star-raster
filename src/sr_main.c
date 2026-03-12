/*  StarRaster — CPU Software Rasterizer
 *  Entry point: Sokol app with a textured neighborhood scene demo.
 */

#if defined(__EMSCRIPTEN__)
    #define SOKOL_GLES3
#else
    #define SOKOL_GLCORE
#endif

#define SOKOL_IMPL
#include "../third_party/sokol/sokol_app.h"
#include "../third_party/sokol/sokol_gfx.h"
#include "../third_party/sokol/sokol_glue.h"
#include "../third_party/sokol/sokol_log.h"

#include "sr_math.h"
#include "sr_raster.h"
#include "sr_texture.h"
#include "sr_gif.h"
#include "sr_font.h"
#include "sr_dungeon.h"

#include <stdio.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
    #include <direct.h>     /* _mkdir */
    #pragma comment(lib, "winmm.lib")   /* timeBeginPeriod */
    #include <mmsystem.h>
#else
    #include <sys/stat.h>   /* mkdir */
    #ifndef __EMSCRIPTEN__
        #include <unistd.h> /* usleep */
    #endif
#endif

/* ── Configuration ───────────────────────────────────────────────── */

#define FB_WIDTH       480
#define FB_HEIGHT      270
#define TARGET_FPS     60
#define GIF_TARGET_FPS 24.0
#define FOG_NEAR       35.0f
#define FOG_FAR        60.0f
#define FOG_COLOR      0xFFEBCE87

/* Directional sun light */
#define SUN_X          0.3f
#define SUN_Y          0.8f
#define SUN_Z          0.5f
#define AMBIENT        0.4f
#define DIFFUSE        0.6f

/* ── Globals ─────────────────────────────────────────────────────── */

static sr_framebuffer fb;

static sg_image    fb_image;
static sg_view     fb_view;
static sg_sampler  fb_sampler;
static sg_pipeline pip;
static sg_bindings bind;
static sg_buffer   vbuf;

enum {
    TEX_BRICK,
    TEX_GRASS,
    TEX_ROOF,
    TEX_WOOD,
    TEX_TILE,
    TEX_COUNT
};
static sr_texture textures[TEX_COUNT];

/* Indexed (palette) textures */
enum {
    ITEX_BRICK,
    ITEX_GRASS,
    ITEX_ROOF,
    ITEX_WOOD,
    ITEX_TILE,
    ITEX_STONE,
    ITEX_COUNT
};
static sr_indexed_texture itextures[ITEX_COUNT];

static double time_acc;
static int    frame_counter;

/* FPS tracking */
static double fps_timer;
static int    fps_frame_count;
static int    fps_display;

/* GIF capture timer */
static double gif_capture_timer;

/* Screenshot counter (for same-second disambiguation) */
static int screenshot_counter;

/* ── Scene / Menu state ─────────────────────────────────────────── */

enum { SCENE_NEIGHBORHOOD, SCENE_CUBES, SCENE_PALETTE_HOUSE, SCENE_DUNGEON, SCENE_COUNT };
enum { STATE_MENU, STATE_RUNNING };

static int  app_state    = STATE_MENU;
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

/* ── 5000 Cubes scene data ──────────────────────────────────────── */

#define NUM_CUBES 5000

typedef struct {
    float x, y, z;
    float rot_y, rot_speed;
    float scale;
    int   tex_id;
} cube_instance;

static cube_instance cube_data[NUM_CUBES];
static bool cubes_initialized = false;

/* ── Shaders ─────────────────────────────────────────────────────── */

#if defined(SOKOL_GLCORE)

static const char *vs_src =
    "#version 330\n"
    "uniform vec2 u_scale;\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos * u_scale, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "}\n";

static const char *fs_src =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, v_uv);\n"
    "}\n";

#elif defined(SOKOL_GLES3)

static const char *vs_src =
    "#version 300 es\n"
    "uniform vec2 u_scale;\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos * u_scale, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "}\n";

static const char *fs_src =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, v_uv);\n"
    "}\n";

#endif

/* ── Lighting system ─────────────────────────────────────────────── */

static bool night_mode = false;

#define NIGHT_SKY_COLOR   0xFF3B1A0A   /* dark navy blue (ABGR) */
#define PAL_NIGHT_SKY     0xFF1E0E0A   /* darker sky for palette scene (ABGR) */
static float pal_ambient    = 0.12f;   /* adjustable ambient for palette scene */
static float pal_light_mult = 1.0f;   /* multiplier for point light RGB */
static bool  adjusting_ambient = true; /* L toggles: true=ambient, false=point light */
static bool  pixel_lighting = false;  /* V toggles: vertex vs pixel lighting */
static bool  shadows_enabled = false; /* S toggles shadow mapping */
#define PAL_DIFFUSE        0.0f        /* no sun in palette night scene */
#define NIGHT_AMBIENT     0.05f

/* ── Shadow map ────────────────────────────────────────────────────── */

#define SHADOW_SIZE  256
#define SHADOW_BIAS  0.008f

static sr_framebuffer shadow_fb;
static sr_mat4        light_vp;

static float shadow_test(float wx, float wy, float wz) {
    if (!shadows_enabled) return 1.0f;
    sr_vec4 lp = sr_mat4_mul_v4(light_vp, sr_v4(wx, wy, wz, 1.0f));
    if (lp.w <= 0.001f) return 1.0f; /* behind light */
    float lx = lp.x / lp.w, ly = lp.y / lp.w, lz = lp.z / lp.w;
    /* NDC to shadow map texel */
    int sx = (int)((lx + 1.0f) * 0.5f * SHADOW_SIZE);
    int sy = (int)((1.0f - ly) * 0.5f * SHADOW_SIZE);
    if (sx < 0 || sx >= SHADOW_SIZE || sy < 0 || sy >= SHADOW_SIZE) return 1.0f;
    float stored_z = shadow_fb.depth[sy * SHADOW_SIZE + sx];
    return (lz - SHADOW_BIAS > stored_z) ? 0.0f : 1.0f;
}

/* Point light */
typedef struct {
    float x, y, z;         /* world position */
    float r, g, b;         /* light color (0-1) */
    float radius;          /* falloff radius */
} point_light;

#define MAX_LIGHTS 16
static point_light lights[MAX_LIGHTS];
static int         num_lights = 0;

/* ── Spatial grid for fast light lookup ───────────────────────────── */

#define GRID_SIZE    8        /* cells per axis */
#define GRID_ORIGIN -32.0f    /* world-space min X/Z */
#define GRID_EXTENT  64.0f    /* world-space total width */
#define GRID_CELL   (GRID_EXTENT / GRID_SIZE)  /* 8.0 units per cell */

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

/* Compute vertex color from directional sun + point lights.
   pos = world-space vertex position, nx/ny/nz = world-space face normal. */
static uint32_t vertex_light(float px, float py, float pz,
                             float nx, float ny, float nz)
{
    float amb = night_mode ? NIGHT_AMBIENT : AMBIENT;
    float lr = amb, lg = amb, lb = amb;

    if (!night_mode) {
        /* Directional sun */
        float len = sqrtf(SUN_X*SUN_X + SUN_Y*SUN_Y + SUN_Z*SUN_Z);
        float sx = SUN_X/len, sy = SUN_Y/len, sz = SUN_Z/len;
        float dot = nx*sx + ny*sy + nz*sz;
        if (dot < 0.0f) dot = 0.0f;
        lr += DIFFUSE * dot;
        lg += DIFFUSE * dot;
        lb += DIFFUSE * dot;
    }

    /* Point lights via spatial grid */
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

/* Convenience: flat face tint (same light at all verts — sun only, no point lights) */
static uint32_t face_tint(float nx, float ny, float nz) {
    return vertex_light(0, 0, 0, nx, ny, nz);
}

/* Face tint for a local-space normal rotated by rot_y around Y */
static uint32_t face_tint_rotY(float lnx, float lny, float lnz, float rot_y) {
    float c = cosf(rot_y), s = sinf(rot_y);
    float wx = lnx * c + lnz * s;
    float wy = lny;
    float wz = -lnx * s + lnz * c;
    return face_tint(wx, wy, wz);
}

/* Compute vertex color for a vertex at world pos, with face normal rotated from local */
static uint32_t vertex_light_rotY(float wpx, float wpy, float wpz,
                                  float lnx, float lny, float lnz, float rot_y) {
    float c = cosf(rot_y), s = sinf(rot_y);
    return vertex_light(wpx, wpy, wpz,
                        lnx*c + lnz*s, lny, -lnx*s + lnz*c);
}

/* ── Unit cube (6 faces = 12 triangles) ──────────────────────────── */

static void draw_cube(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                      const sr_texture *tex)
{
    float s = 0.5f;
    /* Front (+Z) */
    sr_draw_quad(fb_ptr,
        sr_vert(-s, -s,  s,  0, 1), sr_vert(-s,  s,  s,  0, 0),
        sr_vert( s,  s,  s,  1, 0), sr_vert( s, -s,  s,  1, 1), tex, mvp);
    /* Back (-Z) */
    sr_draw_quad(fb_ptr,
        sr_vert( s, -s, -s,  0, 1), sr_vert( s,  s, -s,  0, 0),
        sr_vert(-s,  s, -s,  1, 0), sr_vert(-s, -s, -s,  1, 1), tex, mvp);
    /* Left (-X) */
    sr_draw_quad(fb_ptr,
        sr_vert(-s, -s, -s,  0, 1), sr_vert(-s,  s, -s,  0, 0),
        sr_vert(-s,  s,  s,  1, 0), sr_vert(-s, -s,  s,  1, 1), tex, mvp);
    /* Right (+X) */
    sr_draw_quad(fb_ptr,
        sr_vert( s, -s,  s,  0, 1), sr_vert( s,  s,  s,  0, 0),
        sr_vert( s,  s, -s,  1, 0), sr_vert( s, -s, -s,  1, 1), tex, mvp);
    /* Top (+Y) */
    sr_draw_quad(fb_ptr,
        sr_vert(-s,  s,  s,  0, 1), sr_vert(-s,  s, -s,  0, 0),
        sr_vert( s,  s, -s,  1, 0), sr_vert( s,  s,  s,  1, 1), tex, mvp);
    /* Bottom (-Y) */
    sr_draw_quad(fb_ptr,
        sr_vert(-s, -s, -s,  0, 1), sr_vert(-s, -s,  s,  0, 0),
        sr_vert( s, -s,  s,  1, 0), sr_vert( s, -s, -s,  1, 1), tex, mvp);
}

/* ── Single house (drawn relative to model origin) ───────────────── */

/* Create a lit vertex: local-space pos, UV, face normal (local), transformed via house placement */
static sr_vertex lit_house_vert(float lx, float ly, float lz, float u, float v,
                                float nx, float ny, float nz,
                                float hx, float hz, float rot_y) {
    float c = cosf(rot_y), s = sinf(rot_y);
    float wx = lx*c + lz*s + hx;
    float wy = ly;
    float wz = -lx*s + lz*c + hz;
    uint32_t col = vertex_light_rotY(wx, wy, wz, nx, ny, nz, rot_y);
    return (sr_vertex){ {lx, ly, lz}, {u, v}, col };
}

static void draw_house(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                        float hx, float hz, float rot_y) {
    float W = 2.0f, D = 1.5f, WH = 2.0f, RP = 3.0f;

    /* Shorthand: lit vertex with this house's transform */
    #define LV(lx,ly,lz,u,v,nx,ny,nz) lit_house_vert(lx,ly,lz,u,v,nx,ny,nz,hx,hz,rot_y)

    /* Roof slope normals in local space */
    float slope = (RP - WH) / W;
    float rn_len = sqrtf(1.0f + slope * slope);
    float rnx = 1.0f / rn_len, rny = slope / rn_len;

    /* ── Back wall ────────────────────────────────────────────── */
    sr_draw_quad(fb_ptr,
        LV(-W, 0,  -D,  0,   WH/2,  0,0,-1),
        LV( W, 0,  -D,  W,   WH/2,  0,0,-1),
        LV( W, WH, -D,  W,   0,     0,0,-1),
        LV(-W, WH, -D,  0,   0,     0,0,-1),
        &textures[TEX_BRICK], mvp);

    /* ── Left wall ────────────────────────────────────────────── */
    sr_draw_quad(fb_ptr,
        LV(-W, 0,   D,  0,    WH/2, -1,0,0),
        LV(-W, 0,  -D,  D,    WH/2, -1,0,0),
        LV(-W, WH, -D,  D,    0,    -1,0,0),
        LV(-W, WH,  D,  0,    0,    -1,0,0),
        &textures[TEX_BRICK], mvp);

    /* ── Right wall ───────────────────────────────────────────── */
    sr_draw_quad(fb_ptr,
        LV( W, 0,  -D,  0,    WH/2,  1,0,0),
        LV( W, 0,   D,  D,    WH/2,  1,0,0),
        LV( W, WH,  D,  D,    0,     1,0,0),
        LV( W, WH, -D,  0,    0,     1,0,0),
        &textures[TEX_BRICK], mvp);

    /* ── Front wall with door ─────────────────────────────────── */
    {
        float dhw = 0.4f, dh = 1.6f;

        sr_draw_quad(fb_ptr,
            LV(-dhw, 0,  D,  0,            WH/2, 0,0,1),
            LV(-W,   0,  D,  (W-dhw)/2,    WH/2, 0,0,1),
            LV(-W,   WH, D,  (W-dhw)/2,    0,    0,0,1),
            LV(-dhw, WH, D,  0,            0,    0,0,1),
            &textures[TEX_BRICK], mvp);

        sr_draw_quad(fb_ptr,
            LV( W,   0,  D,  0,            WH/2, 0,0,1),
            LV( dhw, 0,  D,  (W-dhw)/2,    WH/2, 0,0,1),
            LV( dhw, WH, D,  (W-dhw)/2,    0,    0,0,1),
            LV( W,   WH, D,  0,            0,    0,0,1),
            &textures[TEX_BRICK], mvp);

        sr_draw_quad(fb_ptr,
            LV( dhw, dh, D,  0,      (WH-dh)/2, 0,0,1),
            LV(-dhw, dh, D,  dhw,    (WH-dh)/2, 0,0,1),
            LV(-dhw, WH, D,  dhw,    0,         0,0,1),
            LV( dhw, WH, D,  0,      0,         0,0,1),
            &textures[TEX_BRICK], mvp);

        sr_draw_quad(fb_ptr,
            LV( dhw, 0,  D+0.01f, 0,1, 0,0,1),
            LV(-dhw, 0,  D+0.01f, 1,1, 0,0,1),
            LV(-dhw, dh, D+0.01f, 1,0, 0,0,1),
            LV( dhw, dh, D+0.01f, 0,0, 0,0,1),
            &textures[TEX_WOOD], mvp);
    }

    /* ── Gable ends ───────────────────────────────────────────── */
    sr_draw_triangle(fb_ptr,
        LV( W, WH, D,  0,    1, 0,0,1),
        LV(-W, WH, D,  1,    1, 0,0,1),
        LV( 0, RP, D,  0.5f, 0, 0,0,1),
        &textures[TEX_BRICK], mvp);

    sr_draw_triangle(fb_ptr,
        LV(-W, WH, -D, 0,    1, 0,0,-1),
        LV( W, WH, -D, 1,    1, 0,0,-1),
        LV( 0, RP, -D, 0.5f, 0, 0,0,-1),
        &textures[TEX_BRICK], mvp);

    /* ── Roof slopes (double-sided, slope matches gable) ──────── */
    {
        float oh = 0.3f;
        float ey = WH - oh * slope;

        sr_draw_quad_doublesided(fb_ptr,
            LV( 0,    RP, -D-oh,  0,    1, -rnx,rny,0),
            LV( 0,    RP,  D+oh,  1.5f, 1, -rnx,rny,0),
            LV(-W-oh, ey,  D+oh,  1.5f, 0, -rnx,rny,0),
            LV(-W-oh, ey, -D-oh,  0,    0, -rnx,rny,0),
            &textures[TEX_ROOF], mvp);

        sr_draw_quad_doublesided(fb_ptr,
            LV( 0,    RP,  D+oh,  0,    1,  rnx,rny,0),
            LV( 0,    RP, -D-oh,  1.5f, 1,  rnx,rny,0),
            LV( W+oh, ey, -D-oh,  1.5f, 0,  rnx,rny,0),
            LV( W+oh, ey,  D+oh,  0,    0,  rnx,rny,0),
            &textures[TEX_ROOF], mvp);
    }

    /* ── Windows ──────────────────────────────────────────────── */
    {
        float wy = 1.0f, wh = 0.6f, whw = 0.35f, wz = 0.0f;

        sr_draw_quad(fb_ptr,
            LV(-W-0.01f, wy,    wz+whw, 0,1, -1,0,0),
            LV(-W-0.01f, wy,    wz-whw, 1,1, -1,0,0),
            LV(-W-0.01f, wy+wh, wz-whw, 1,0, -1,0,0),
            LV(-W-0.01f, wy+wh, wz+whw, 0,0, -1,0,0),
            &textures[TEX_TILE], mvp);

        sr_draw_quad(fb_ptr,
            LV(W+0.01f, wy,    wz-whw, 0,1, 1,0,0),
            LV(W+0.01f, wy,    wz+whw, 1,1, 1,0,0),
            LV(W+0.01f, wy+wh, wz+whw, 1,0, 1,0,0),
            LV(W+0.01f, wy+wh, wz-whw, 0,0, 1,0,0),
            &textures[TEX_TILE], mvp);
    }

    #undef LV
}

/* ── Neighborhood layout ─────────────────────────────────────────── */

/* House placement: position (x,z) and rotation angle (radians) */
typedef struct { float x, z, rot; } house_placement;

static const house_placement houses[] = {
    /* Row 1 (Z = -14) — houses face +Z (toward street) */
    { -12.0f, -14.0f,  0.0f },
    {   0.0f, -14.0f,  0.0f },
    {  12.0f, -14.0f,  0.0f },
    /* Row 2 (Z = -6) — houses face -Z (toward street) */
    { -12.0f,  -6.0f,  3.14159f },
    {   0.0f,  -6.0f,  3.14159f },
    {  12.0f,  -6.0f,  3.14159f },
    /* Row 3 (Z = 6) — houses face +Z */
    { -12.0f,   6.0f,  0.0f },
    {   0.0f,   6.0f,  0.0f },
    {  12.0f,   6.0f,  0.0f },
    /* Row 4 (Z = 14) — houses face -Z */
    { -12.0f,  14.0f,  3.14159f },
    {   0.0f,  14.0f,  3.14159f },
    {  12.0f,  14.0f,  3.14159f },
};
#define NUM_HOUSES (sizeof(houses) / sizeof(houses[0]))

/* Draw a lit ground quad (Gouraud: each corner gets its own vertex_light) */
static void draw_ground_quad(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                              float x0, float z0, float x1, float z1,
                              float u0, float v0, float u1, float v1,
                              float y, const sr_texture *tex) {
    uint32_t c00 = vertex_light(x0, y, z0, 0,1,0);
    uint32_t c10 = vertex_light(x1, y, z0, 0,1,0);
    uint32_t c11 = vertex_light(x1, y, z1, 0,1,0);
    uint32_t c01 = vertex_light(x0, y, z1, 0,1,0);
    sr_draw_quad(fb_ptr,
        sr_vert_c(x0, y, z1,  u0, v1, c01),
        sr_vert_c(x0, y, z0,  u0, v0, c00),
        sr_vert_c(x1, y, z0,  u1, v0, c10),
        sr_vert_c(x1, y, z1,  u1, v1, c11),
        tex, vp);
}

/* Draw a street rectangle as tiled quads for better Gouraud resolution */
static void draw_street_tiled(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                               float x0, float z0, float x1, float z1,
                               float y, float tile) {
    for (float tx = x0; tx < x1; tx += tile) {
        float nx = tx + tile;
        if (nx > x1) nx = x1;
        for (float tz = z0; tz < z1; tz += tile) {
            float nz = tz + tile;
            if (nz > z1) nz = z1;
            draw_ground_quad(fb_ptr, vp, tx, tz, nx, nz,
                             0, 0, (nx-tx)*0.5f, (nz-tz)*0.5f, y, &textures[TEX_TILE]);
        }
    }
}

/* ── Street light setup (night mode) ─────────────────────────────── */

static void setup_street_lights(float t) {
    num_lights = 0;
    if (!night_mode) return;

    /* Lights patrol along horizontal streets at z = -10, 0, 10 */
    float hz[] = { -10.0f, 0.0f, 10.0f };
    for (int i = 0; i < 3; i++) {
        /* Two lights per street, moving opposite directions */
        float speed = 6.0f;
        float range = 22.0f;
        float x1 = fmodf(t * speed + i * 8.0f, range * 2.0f) - range;
        float x2 = fmodf(-t * speed + i * 5.0f + 15.0f, range * 2.0f) - range;
        lights[num_lights++] = (point_light){ x1, 3.0f, hz[i], 1.0f, 0.85f, 0.5f, 12.0f };
        lights[num_lights++] = (point_light){ x2, 3.0f, hz[i], 1.0f, 0.85f, 0.5f, 12.0f };
    }

    /* Lights patrol along vertical streets at x = -6, 6 */
    float vx[] = { -6.0f, 6.0f };
    for (int i = 0; i < 2; i++) {
        float speed = 5.0f;
        float range = 20.0f;
        float z1 = fmodf(t * speed + i * 12.0f, range * 2.0f) - range;
        float z2 = fmodf(-t * speed + i * 7.0f + 10.0f, range * 2.0f) - range;
        lights[num_lights++] = (point_light){ vx[i], 3.0f, z1, 0.8f, 0.7f, 1.0f, 10.0f };
        lights[num_lights++] = (point_light){ vx[i], 3.0f, z2, 0.8f, 0.7f, 1.0f, 10.0f };
    }
}

static void draw_neighborhood(sr_framebuffer *fb_ptr, const sr_mat4 *vp) {
    /* ── Ground plane (grass) — tiled for near-plane clipping ── */
    {
        float G = 30.0f;
        int   tiles = 10;
        float ts = (2.0f * G) / tiles;
        for (int tz = 0; tz < tiles; tz++) {
            for (int tx = 0; tx < tiles; tx++) {
                float x0 = -G + tx * ts, x1 = x0 + ts;
                float z0 = -G + tz * ts, z1 = z0 + ts;
                float u0 = (x0 + G) * 0.5f, u1 = (x1 + G) * 0.5f;
                float v0 = (z0 + G) * 0.5f, v1 = (z1 + G) * 0.5f;
                draw_ground_quad(fb_ptr, vp, x0, z0, x1, z1,
                                 u0, v0, u1, v1, 0.0f, &textures[TEX_GRASS]);
            }
        }
    }

    /* ── Streets (tiled, no overlap at intersections) ───────── */
    float sw = 1.2f;             /* street half-width */
    float sy = 0.05f;            /* street elevation */
    float tile = 2.4f;           /* tile size for Gouraud vertex density */
    float h_len = 25.0f;
    float v_len = 22.0f;
    float hz[] = { -10.0f, 0.0f, 10.0f };
    float vx[] = {  -6.0f, 6.0f };

    /* Horizontal streets (continuous, tiled) */
    for (int i = 0; i < 3; i++) {
        draw_street_tiled(fb_ptr, vp, -h_len, hz[i]-sw, h_len, hz[i]+sw,
                          sy, tile);
    }

    /* Vertical streets (segmented at intersections, tiled) */
    for (int i = 0; i < 2; i++) {
        float segs[][2] = {
            { -v_len,      hz[0] - sw },
            { hz[0] + sw,  hz[1] - sw },
            { hz[1] + sw,  hz[2] - sw },
            { hz[2] + sw,  v_len      },
        };
        for (int j = 0; j < 4; j++) {
            draw_street_tiled(fb_ptr, vp, vx[i]-sw, segs[j][0], vx[i]+sw, segs[j][1],
                              sy, tile);
        }
    }

    /* ── Houses ───────────────────────────────────────────────── */
    for (int i = 0; i < (int)NUM_HOUSES; i++) {
        sr_mat4 model = sr_mat4_mul(
            sr_mat4_translate(houses[i].x, 0, houses[i].z),
            sr_mat4_rotate_y(houses[i].rot)
        );
        sr_mat4 mvp = sr_mat4_mul(*vp, model);
        draw_house(fb_ptr, &mvp, houses[i].x, houses[i].z, houses[i].rot);
    }
}

/* ── 5000 Cubes scene ────────────────────────────────────────────── */

static void cubes_scene_init(void) {
    if (cubes_initialized) return;
    rng_state = 42;
    for (int i = 0; i < NUM_CUBES; i++) {
        cube_data[i].x = rng_range(-40.0f, 40.0f);
        cube_data[i].y = rng_range( 0.5f, 25.0f);
        cube_data[i].z = rng_range(-40.0f, 40.0f);
        cube_data[i].rot_y = rng_range(0, 6.283f);
        cube_data[i].rot_speed = rng_range(-2.0f, 2.0f);
        cube_data[i].scale = rng_range(0.3f, 1.5f);
        cube_data[i].tex_id = (int)(rng_float() * TEX_COUNT) % TEX_COUNT;
    }
    cubes_initialized = true;
}

static void draw_cube_scene(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                             float t)
{
    /* Ground plane — tiled */
    uint32_t t_ground = face_tint(0, 1, 0);
    {
        float G = 50.0f;
        int   tiles = 12;
        float ts = (2.0f * G) / tiles;
        for (int tz = 0; tz < tiles; tz++) {
            for (int tx = 0; tx < tiles; tx++) {
                float x0 = -G + tx * ts, x1 = x0 + ts;
                float z0 = -G + tz * ts, z1 = z0 + ts;
                sr_draw_quad(fb_ptr,
                    sr_vert_c(x0, 0, z1, 0,ts, t_ground), sr_vert_c(x0, 0, z0, 0, 0, t_ground),
                    sr_vert_c(x1, 0, z0, ts,0, t_ground), sr_vert_c(x1, 0, z1, ts,ts, t_ground),
                    &textures[TEX_GRASS], vp);
            }
        }
    }

    /* 5000 spinning cubes */
    for (int i = 0; i < NUM_CUBES; i++) {
        float angle = cube_data[i].rot_y + t * cube_data[i].rot_speed;
        float s = cube_data[i].scale;
        sr_mat4 model = sr_mat4_mul(
            sr_mat4_mul(
                sr_mat4_translate(cube_data[i].x, cube_data[i].y, cube_data[i].z),
                sr_mat4_rotate_y(angle)
            ),
            sr_mat4_scale(s, s, s)
        );
        sr_mat4 mvp = sr_mat4_mul(*vp, model);
        draw_cube(fb_ptr, &mvp, &textures[cube_data[i].tex_id]);
    }
}

/* ── Palette House scene ─────────────────────────────────────────── */

/* Encode lighting intensity into vertex color R channel.
 * intensity: 0.0 = full dark, 1.0 = default, 2.0 = full bright */
static inline uint32_t pal_intensity_color(float intensity) {
    int val = (int)(intensity * 128.0f);
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    return 0xFF000000 | (uint32_t)val | ((uint32_t)val << 8) | ((uint32_t)val << 16);
}

/* Compute palette lighting intensity at a world position from point lights only.
 * Night scene: low ambient, no sun — point lights are the only source.
 * Returns 0.0 (darkest) to ~2.0 (brightest). */
static float pal_vertex_intensity(float px, float py, float pz,
                                   float nx, float ny, float nz)
{
    float total = pal_ambient;
    float shadow = shadow_test(px, py, pz);

    /* Point lights via spatial grid */
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
            /* Average the light's RGB contribution as a single intensity */
            float lum = (lights[i].r + lights[i].g + lights[i].b) * (1.0f / 3.0f);
            total += lum * atten * ldot * shadow;
        }
    }

    return total;
}

static sr_vertex pal_house_vert(float lx, float ly, float lz, float u, float v,
                                 float nx, float ny, float nz,
                                 float hx, float hz, float rot_y) {
    float c = cosf(rot_y), s = sinf(rot_y);
    float wx = lx*c + lz*s + hx;
    float wy = ly;
    float wz = -lx*s + lz*c + hz;
    /* Rotate normal */
    float wnx = nx*c + nz*s, wny = ny, wnz = -nx*s + nz*c;
    float intensity = pal_vertex_intensity(wx, wy, wz, wnx, wny, wnz);
    uint32_t col = pal_intensity_color(intensity);
    return sr_vert_world(lx, ly, lz, u, v, col, wx, wy, wz, wnx, wny, wnz);
}

static void draw_palette_house(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                float hx, float hz, float rot_y) {
    float W = 2.0f, D = 1.5f, WH = 2.0f, RP = 3.0f;

    #define PV(lx,ly,lz,u,v,nx,ny,nz) pal_house_vert(lx,ly,lz,u,v,nx,ny,nz,hx,hz,rot_y)

    /* Select vertex-lit or pixel-lit draw functions */
    void (*draw_quad)(sr_framebuffer*, sr_vertex, sr_vertex, sr_vertex, sr_vertex,
                      const sr_indexed_texture*, const sr_mat4*) =
        pixel_lighting ? sr_draw_quad_indexed_pixellit : sr_draw_quad_indexed;
    void (*draw_tri)(sr_framebuffer*, sr_vertex, sr_vertex, sr_vertex,
                     const sr_indexed_texture*, const sr_mat4*) =
        pixel_lighting ? sr_draw_triangle_indexed_pixellit : sr_draw_triangle_indexed;
    void (*draw_quad_ds)(sr_framebuffer*, sr_vertex, sr_vertex, sr_vertex, sr_vertex,
                         const sr_indexed_texture*, const sr_mat4*) =
        pixel_lighting ? sr_draw_quad_indexed_doublesided_pixellit : sr_draw_quad_indexed_doublesided;

    float slope = (RP - WH) / W;
    float rn_len = sqrtf(1.0f + slope * slope);
    float rnx = 1.0f / rn_len, rny = slope / rn_len;

    /* Back wall */
    draw_quad(fb_ptr,
        PV(-W, 0,  -D,  0,   WH/2,  0,0,-1),
        PV( W, 0,  -D,  W,   WH/2,  0,0,-1),
        PV( W, WH, -D,  W,   0,     0,0,-1),
        PV(-W, WH, -D,  0,   0,     0,0,-1),
        &itextures[ITEX_BRICK], mvp);

    /* Left wall */
    draw_quad(fb_ptr,
        PV(-W, 0,   D,  0,    WH/2, -1,0,0),
        PV(-W, 0,  -D,  D,    WH/2, -1,0,0),
        PV(-W, WH, -D,  D,    0,    -1,0,0),
        PV(-W, WH,  D,  0,    0,    -1,0,0),
        &itextures[ITEX_BRICK], mvp);

    /* Right wall */
    draw_quad(fb_ptr,
        PV( W, 0,  -D,  0,    WH/2,  1,0,0),
        PV( W, 0,   D,  D,    WH/2,  1,0,0),
        PV( W, WH,  D,  D,    0,     1,0,0),
        PV( W, WH, -D,  0,    0,     1,0,0),
        &itextures[ITEX_BRICK], mvp);

    /* Front wall (simplified, no door cutout for this demo) */
    draw_quad(fb_ptr,
        PV( W, 0,   D,  0,   WH/2, 0,0,1),
        PV(-W, 0,   D,  W,   WH/2, 0,0,1),
        PV(-W, WH,  D,  W,   0,    0,0,1),
        PV( W, WH,  D,  0,   0,    0,0,1),
        &itextures[ITEX_BRICK], mvp);

    /* Door */
    {
        float dhw = 0.4f, dh = 1.6f;
        draw_quad(fb_ptr,
            PV( dhw, 0,  D+0.01f, 0,1, 0,0,1),
            PV(-dhw, 0,  D+0.01f, 1,1, 0,0,1),
            PV(-dhw, dh, D+0.01f, 1,0, 0,0,1),
            PV( dhw, dh, D+0.01f, 0,0, 0,0,1),
            &itextures[ITEX_WOOD], mvp);
    }

    /* Gable ends */
    draw_tri(fb_ptr,
        PV( W, WH, D,  0,    1, 0,0,1),
        PV(-W, WH, D,  1,    1, 0,0,1),
        PV( 0, RP, D,  0.5f, 0, 0,0,1),
        &itextures[ITEX_BRICK], mvp);

    draw_tri(fb_ptr,
        PV(-W, WH, -D, 0,    1, 0,0,-1),
        PV( W, WH, -D, 1,    1, 0,0,-1),
        PV( 0, RP, -D, 0.5f, 0, 0,0,-1),
        &itextures[ITEX_BRICK], mvp);

    /* Roof slopes */
    {
        float oh = 0.3f;
        float ey = WH - oh * slope;

        draw_quad_ds(fb_ptr,
            PV( 0,    RP, -D-oh,  0,    1, -rnx,rny,0),
            PV( 0,    RP,  D+oh,  1.5f, 1, -rnx,rny,0),
            PV(-W-oh, ey,  D+oh,  1.5f, 0, -rnx,rny,0),
            PV(-W-oh, ey, -D-oh,  0,    0, -rnx,rny,0),
            &itextures[ITEX_ROOF], mvp);

        draw_quad_ds(fb_ptr,
            PV( 0,    RP,  D+oh,  0,    1,  rnx,rny,0),
            PV( 0,    RP, -D-oh,  1.5f, 1,  rnx,rny,0),
            PV( W+oh, ey, -D-oh,  1.5f, 0,  rnx,rny,0),
            PV( W+oh, ey,  D+oh,  0,    0,  rnx,rny,0),
            &itextures[ITEX_ROOF], mvp);
    }

    /* Windows */
    {
        float wy = 1.0f, wh = 0.6f, whw = 0.35f, wz = 0.0f;

        draw_quad(fb_ptr,
            PV(-W-0.01f, wy,    wz+whw, 0,1, -1,0,0),
            PV(-W-0.01f, wy,    wz-whw, 1,1, -1,0,0),
            PV(-W-0.01f, wy+wh, wz-whw, 1,0, -1,0,0),
            PV(-W-0.01f, wy+wh, wz+whw, 0,0, -1,0,0),
            &itextures[ITEX_TILE], mvp);

        draw_quad(fb_ptr,
            PV(W+0.01f, wy,    wz-whw, 0,1, 1,0,0),
            PV(W+0.01f, wy,    wz+whw, 1,1, 1,0,0),
            PV(W+0.01f, wy+wh, wz+whw, 1,0, 1,0,0),
            PV(W+0.01f, wy+wh, wz-whw, 0,0, 1,0,0),
            &itextures[ITEX_TILE], mvp);
    }

    #undef PV
}

/* ── Wireframe sphere ────────────────────────────────────────────── */

static void draw_line_3d(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                          float x0, float y0, float z0,
                          float x1, float y1, float z1, uint32_t color)
{
    int W = fb_ptr->width, H = fb_ptr->height;
    /* Transform both endpoints */
    sr_vec4 c0 = sr_mat4_mul_v4(*mvp, (sr_vec4){x0, y0, z0, 1.0f});
    sr_vec4 c1 = sr_mat4_mul_v4(*mvp, (sr_vec4){x1, y1, z1, 1.0f});

    /* Clip: skip if both behind near plane */
    if (c0.w < 0.001f && c1.w < 0.001f) return;

    /* Simple clip: skip lines partially behind camera for now */
    if (c0.w < 0.001f || c1.w < 0.001f) return;

    /* NDC → screen */
    float sx0 = (c0.x / c0.w + 1.0f) * 0.5f * W;
    float sy0 = (1.0f - c0.y / c0.w) * 0.5f * H;
    float sx1 = (c1.x / c1.w + 1.0f) * 0.5f * W;
    float sy1 = (1.0f - c1.y / c1.w) * 0.5f * H;

    /* Bresenham */
    int ix0 = (int)sx0, iy0 = (int)sy0;
    int ix1 = (int)sx1, iy1 = (int)sy1;

    int dx = abs(ix1 - ix0), dy = -abs(iy1 - iy0);
    int step_x = ix0 < ix1 ? 1 : -1;
    int step_y = iy0 < iy1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        if (ix0 >= 0 && ix0 < W && iy0 >= 0 && iy0 < H)
            fb_ptr->color[iy0 * W + ix0] = color;
        if (ix0 == ix1 && iy0 == iy1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; ix0 += step_x; }
        if (e2 <= dx) { err += dx; iy0 += step_y; }
    }
}

static void draw_wireframe_sphere(sr_framebuffer *fb_ptr, const sr_mat4 *vp,
                                    float cx, float cy, float cz, float radius,
                                    uint32_t color, int segments)
{
    float pi = 3.14159265f;
    /* Draw 3 great circles: XY, XZ, YZ */
    for (int i = 0; i < segments; i++) {
        float a0 = 2.0f * pi * i / segments;
        float a1 = 2.0f * pi * (i + 1) / segments;
        float c0a = cosf(a0), s0a = sinf(a0);
        float c1a = cosf(a1), s1a = sinf(a1);

        /* XZ ring (horizontal) */
        draw_line_3d(fb_ptr, vp,
            cx + radius * c0a, cy, cz + radius * s0a,
            cx + radius * c1a, cy, cz + radius * s1a, color);
        /* XY ring (vertical front) */
        draw_line_3d(fb_ptr, vp,
            cx + radius * c0a, cy + radius * s0a, cz,
            cx + radius * c1a, cy + radius * s1a, cz, color);
        /* YZ ring (vertical side) */
        draw_line_3d(fb_ptr, vp,
            cx, cy + radius * c0a, cz + radius * s0a,
            cx, cy + radius * c1a, cz + radius * s1a, color);
    }
}

/* Render depth-only scene into shadow map from light's perspective */
static void draw_palette_scene_depth_only(sr_framebuffer *sm, const sr_mat4 *lvp) {
    /* Ground plane */
    {
        float G = 10.0f;
        int tiles = 6;
        float ts = (2.0f * G) / tiles;
        for (int tz = 0; tz < tiles; tz++) {
            for (int tx = 0; tx < tiles; tx++) {
                float x0 = -G + tx * ts, x1 = x0 + ts;
                float z0 = -G + tz * ts, z1 = z0 + ts;
                sr_draw_quad_depth_only(sm,
                    sr_vert(x0, 0, z1, 0, 0),
                    sr_vert(x0, 0, z0, 0, 0),
                    sr_vert(x1, 0, z0, 0, 0),
                    sr_vert(x1, 0, z1, 0, 0),
                    lvp);
            }
        }
    }
    /* House at origin */
    {
        float W = 2.0f, D = 1.5f, WH = 2.0f, RP = 3.0f;
        float slope = (RP - WH) / W;
        float oh = 0.3f, ey = WH - oh * slope;
        #define DV(x,y,z) sr_vert(x,y,z, 0,0)

        /* Walls */
        sr_draw_quad_depth_only(sm, DV(-W,0,-D), DV(W,0,-D), DV(W,WH,-D), DV(-W,WH,-D), lvp);
        sr_draw_quad_depth_only(sm, DV(-W,0,D), DV(-W,0,-D), DV(-W,WH,-D), DV(-W,WH,D), lvp);
        sr_draw_quad_depth_only(sm, DV(W,0,-D), DV(W,0,D), DV(W,WH,D), DV(W,WH,-D), lvp);
        sr_draw_quad_depth_only(sm, DV(W,0,D), DV(-W,0,D), DV(-W,WH,D), DV(W,WH,D), lvp);

        /* Gable ends */
        sr_draw_triangle_depth_only(sm, DV(W,WH,D), DV(-W,WH,D), DV(0,RP,D), lvp);
        sr_draw_triangle_depth_only(sm, DV(-W,WH,-D), DV(W,WH,-D), DV(0,RP,-D), lvp);

        /* Roof slopes (both sides) */
        sr_draw_quad_depth_only(sm, DV(0,RP,-D-oh), DV(0,RP,D+oh), DV(-W-oh,ey,D+oh), DV(-W-oh,ey,-D-oh), lvp);
        sr_draw_quad_depth_only(sm, DV(0,RP,D+oh), DV(0,RP,-D-oh), DV(W+oh,ey,-D-oh), DV(W+oh,ey,D+oh), lvp);

        #undef DV
    }
}

/* Pixel lighting callback wrapper for pal_vertex_intensity */
static float pixel_light_callback(float px, float py, float pz,
                                   float nx, float ny, float nz) {
    return pal_vertex_intensity(px, py, pz, nx, ny, nz);
}

static void draw_palette_scene(sr_framebuffer *fb_ptr, const sr_mat4 *vp, float t) {
    /* Set up orbiting point light FIRST (needed for both vertex and pixel lighting) */
    num_lights = 1;
    float orbit_radius = 5.0f;
    float lx = cosf(t * 1.2f) * orbit_radius;
    float lz = sinf(t * 1.2f) * orbit_radius;
    float ly = 2.5f + sinf(t * 0.7f) * 1.0f;
    lights[0] = (point_light){ lx, ly, lz,
        2.0f * pal_light_mult, 1.8f * pal_light_mult, 1.2f * pal_light_mult, 12.0f };
    build_light_grid();

    /* Shadow map pass */
    if (shadows_enabled) {
        sr_mat4 light_view = sr_mat4_lookat(sr_v3(lx, ly, lz), sr_v3(0, 0, 0), sr_v3(0, 1, 0));
        sr_mat4 light_proj = sr_mat4_perspective(1.5f, 1.0f, 0.5f, 30.0f);
        light_vp = sr_mat4_mul(light_proj, light_view);
        sr_framebuffer_clear(&shadow_fb, 0, 1.0f);
        draw_palette_scene_depth_only(&shadow_fb, &light_vp);
    }

    /* Set pixel lighting callback */
    if (pixel_lighting)
        sr_set_pixel_light_fn(pixel_light_callback);

    /* Ground plane (grass) */
    {
        float G = 10.0f;
        int tiles = 6;
        float ts = (2.0f * G) / tiles;
        for (int tz = 0; tz < tiles; tz++) {
            for (int tx = 0; tx < tiles; tx++) {
                float x0 = -G + tx * ts, x1 = x0 + ts;
                float z0 = -G + tz * ts, z1 = z0 + ts;

                float i00 = pal_vertex_intensity(x0, 0, z0, 0,1,0);
                float i10 = pal_vertex_intensity(x1, 0, z0, 0,1,0);
                float i11 = pal_vertex_intensity(x1, 0, z1, 0,1,0);
                float i01 = pal_vertex_intensity(x0, 0, z1, 0,1,0);

                if (pixel_lighting) {
                    sr_draw_quad_indexed_pixellit(fb_ptr,
                        sr_vert_world(x0, 0, z1, 0,ts, pal_intensity_color(i01), x0,0,z1, 0,1,0),
                        sr_vert_world(x0, 0, z0, 0, 0, pal_intensity_color(i00), x0,0,z0, 0,1,0),
                        sr_vert_world(x1, 0, z0, ts,0, pal_intensity_color(i10), x1,0,z0, 0,1,0),
                        sr_vert_world(x1, 0, z1, ts,ts, pal_intensity_color(i11), x1,0,z1, 0,1,0),
                        &itextures[ITEX_GRASS], vp);
                } else {
                    sr_draw_quad_indexed(fb_ptr,
                        sr_vert_c(x0, 0, z1, 0,ts, pal_intensity_color(i01)),
                        sr_vert_c(x0, 0, z0, 0, 0, pal_intensity_color(i00)),
                        sr_vert_c(x1, 0, z0, ts,0, pal_intensity_color(i10)),
                        sr_vert_c(x1, 0, z1, ts,ts, pal_intensity_color(i11)),
                        &itextures[ITEX_GRASS], vp);
                }
            }
        }
    }

    /* Single house at origin */
    sr_mat4 mvp = *vp;  /* house at origin, no transform */
    draw_palette_house(fb_ptr, &mvp, 0.0f, 0.0f, 0.0f);

    /* Draw wireframe sphere at light position */
    draw_wireframe_sphere(fb_ptr, vp, lx, ly, lz, 0.3f, 0xFF55CCFF, 16);
}

/* ── Dungeon Crawler scene ───────────────────────────────────────── */

#include "sr_config.h"

/* Runtime dungeon config (loaded from config/dungeon.yaml) */
static struct {
    float light_color[3];
    float light_brightness;
    float light_min_range;
    float light_attn_dist;
    float ambient_color[3];
    float ambient_brightness;
    int   fog_levels;
    float fog_start[16];
    float fog_intensity[16];
    float fog_density[16];
    float fog_stop;
} dng_cfg;

static void dng_load_config(void) {
    sr_config cfg = sr_config_load("config/dungeon.yaml");

    /* Torch */
    float color[3] = {1,1,1};
    sr_config_array(&cfg, "torch.color", color, 3);
    dng_cfg.light_color[0] = color[0];
    dng_cfg.light_color[1] = color[1];
    dng_cfg.light_color[2] = color[2];
    dng_cfg.light_brightness = sr_config_float(&cfg, "torch.brightness", 1.0f);
    dng_cfg.light_min_range  = sr_config_float(&cfg, "torch.min_range", 1.0f);
    dng_cfg.light_attn_dist  = sr_config_float(&cfg, "torch.attn_dist", 6.0f);

    /* Ambient */
    float amb[3] = {0.15f, 0.12f, 0.18f};
    sr_config_array(&cfg, "ambient.color", amb, 3);
    dng_cfg.ambient_color[0] = amb[0];
    dng_cfg.ambient_color[1] = amb[1];
    dng_cfg.ambient_color[2] = amb[2];
    dng_cfg.ambient_brightness = sr_config_float(&cfg, "ambient.brightness", 0.1f);

    /* Fog */
    dng_cfg.fog_levels = (int)sr_config_float(&cfg, "fog.levels", 5.0f);
    if (dng_cfg.fog_levels > 16) dng_cfg.fog_levels = 16;
    sr_config_array(&cfg, "fog.start", dng_cfg.fog_start, dng_cfg.fog_levels);
    sr_config_array(&cfg, "fog.intensity", dng_cfg.fog_intensity, dng_cfg.fog_levels);
    sr_config_array(&cfg, "fog.density", dng_cfg.fog_density, dng_cfg.fog_levels);
    dng_cfg.fog_stop = sr_config_float(&cfg, "fog.stop", 8.0f);

    sr_config_free(&cfg);
    printf("[dungeon] Config loaded: torch(%.1f/%.1f/%.1f) ambient(%.2f) fog(%d levels)\n",
           dng_cfg.light_brightness, dng_cfg.light_min_range, dng_cfg.light_attn_dist,
           dng_cfg.ambient_brightness, dng_cfg.fog_levels);
}

static dng_game dng_state;
static bool dng_initialized = false;

enum {
    DNG_STATE_PLAYING,
    DNG_STATE_CLIMBING,
};
static int dng_play_state = DNG_STATE_PLAYING;

/* Lighting mode: 0 = torch (point light + ambient), 1 = depth fog (dithered bands) */
static int dng_light_mode = 0;


/* ── Dungeon torch lighting (pixel-lit callback) ─────────────────── */

/* Per-pixel light intensity from the player torch + ambient.
 * Used as sr_pixel_light_fn callback for pixel-lit path. */
static float dng_torch_light(float wx, float wy, float wz,
                              float nx, float ny, float nz)
{
    dng_player *p = &dng_state.player;

    /* Ambient base */
    float ambient = dng_cfg.ambient_brightness *
        (dng_cfg.ambient_color[0] + dng_cfg.ambient_color[1] + dng_cfg.ambient_color[2]) / 3.0f;

    /* Torch: point light at player position */
    float dx = p->x - wx;
    float dy = p->y - wy;
    float dz = p->z - wz;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz);

    if (dist >= dng_cfg.light_attn_dist) return ambient;

    /* Attenuation: full brightness within min_range, then linear falloff */
    float atten;
    if (dist <= dng_cfg.light_min_range) {
        atten = 1.0f;
    } else {
        atten = 1.0f - (dist - dng_cfg.light_min_range) /
                        (dng_cfg.light_attn_dist - dng_cfg.light_min_range);
        atten *= atten; /* quadratic falloff */
    }

    /* N dot L */
    float inv_dist = 1.0f / (dist + 0.001f);
    float ndotl = (dx * nx + dy * ny + dz * nz) * inv_dist;
    if (ndotl < 0.0f) ndotl = 0.0f;

    float torch_lum = dng_cfg.light_brightness *
        (dng_cfg.light_color[0] + dng_cfg.light_color[1] + dng_cfg.light_color[2]) / 3.0f;

    return ambient + torch_lum * atten * ndotl;
}

/* ── Dungeon depth-fog intensity ─────────────────────────────────── */

/* Compute shade intensity from distance to player (for vertex-lit fog mode).
 * Returns a value suitable for pal_intensity_color(). */
static float dng_fog_intensity_at_dist(float dist_cells) {
    /* Find which fog band we're in */
    if (dist_cells < dng_cfg.fog_start[0]) return dng_cfg.fog_intensity[0];

    for (int i = 0; i < dng_cfg.fog_levels - 1; i++) {
        if (dist_cells < dng_cfg.fog_start[i + 1]) {
            /* Lerp between this band and the next */
            float t = (dist_cells - dng_cfg.fog_start[i]) /
                      (dng_cfg.fog_start[i + 1] - dng_cfg.fog_start[i]);
            return dng_cfg.fog_intensity[i] + t * (dng_cfg.fog_intensity[i + 1] - dng_cfg.fog_intensity[i]);
        }
    }
    return dng_cfg.fog_intensity[dng_cfg.fog_levels - 1];
}

/* Compute vertex intensity for fog mode based on distance to player */
static float dng_fog_vertex_intensity(float wx, float wy, float wz) {
    dng_player *p = &dng_state.player;
    float dx = p->x - wx;
    float dy = p->y - wy;
    float dz = p->z - wz;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz) / DNG_CELL_SIZE; /* in cells */
    return dng_fog_intensity_at_dist(dist);
}

/* Draw a textured wall quad in the dungeon.
 * In torch mode: pixel-lit with normals. In fog mode: vertex-lit by distance.
 * nx,ny,nz = face normal for pixel lighting. */
static void dng_draw_wall(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                           float ax, float ay, float az,
                           float bx, float by, float bz,
                           float cx, float cy, float cz,
                           float dx, float dy, float dz,
                           const sr_indexed_texture *tex,
                           float nx, float ny, float nz) {
    /* Compute UVs proportional to actual quad dimensions */
    float edge_x = bx - ax, edge_y = by - ay, edge_z = bz - az;
    float width = sqrtf(edge_x*edge_x + edge_y*edge_y + edge_z*edge_z);
    float vert_x = dx - ax, vert_y = dy - ay, vert_z = dz - az;
    float height = sqrtf(vert_x*vert_x + vert_y*vert_y + vert_z*vert_z);
    float u_scale = width / DNG_CELL_SIZE;
    float v_scale = height / DNG_CELL_SIZE;

    if (dng_light_mode == 0) {
        /* Torch mode: pixel-lit with world pos + normal */
        sr_draw_quad_indexed_pixellit(fb_ptr,
            sr_vert_world(ax,ay,az, 0,0, 0xFFFFFFFF, ax,ay,az, nx,ny,nz),
            sr_vert_world(bx,by,bz, u_scale,0, 0xFFFFFFFF, bx,by,bz, nx,ny,nz),
            sr_vert_world(cx,cy,cz, u_scale,v_scale, 0xFFFFFFFF, cx,cy,cz, nx,ny,nz),
            sr_vert_world(dx,dy,dz, 0,v_scale, 0xFFFFFFFF, dx,dy,dz, nx,ny,nz),
            tex, mvp);
    } else {
        /* Fog mode: vertex-lit by distance */
        uint32_t ca = pal_intensity_color(dng_fog_vertex_intensity(ax,ay,az));
        uint32_t cb = pal_intensity_color(dng_fog_vertex_intensity(bx,by,bz));
        uint32_t cc = pal_intensity_color(dng_fog_vertex_intensity(cx,cy,cz));
        uint32_t cd = pal_intensity_color(dng_fog_vertex_intensity(dx,dy,dz));
        sr_draw_quad_indexed(fb_ptr,
            sr_vert_c(ax,ay,az, 0,0, ca),
            sr_vert_c(bx,by,bz, u_scale,0, cb),
            sr_vert_c(cx,cy,cz, u_scale,v_scale, cc),
            sr_vert_c(dx,dy,dz, 0,v_scale, cd),
            tex, mvp);
    }
}

/* Draw a horizontal quad (floor or ceiling) with proper lighting */
static void dng_draw_hquad(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                            float ax, float ay, float az,
                            float bx, float by, float bz,
                            float cx, float cy, float cz,
                            float dx, float dy, float dz,
                            float u0, float v0, float u1, float v1,
                            const sr_indexed_texture *tex,
                            float nx, float ny, float nz) {
    if (dng_light_mode == 0) {
        sr_draw_quad_indexed_pixellit(fb_ptr,
            sr_vert_world(ax,ay,az, u0,v0, 0xFFFFFFFF, ax,ay,az, nx,ny,nz),
            sr_vert_world(bx,by,bz, u1,v0, 0xFFFFFFFF, bx,by,bz, nx,ny,nz),
            sr_vert_world(cx,cy,cz, u1,v1, 0xFFFFFFFF, cx,cy,cz, nx,ny,nz),
            sr_vert_world(dx,dy,dz, u0,v1, 0xFFFFFFFF, dx,dy,dz, nx,ny,nz),
            tex, mvp);
    } else {
        uint32_t ca = pal_intensity_color(dng_fog_vertex_intensity(ax,ay,az));
        uint32_t cb = pal_intensity_color(dng_fog_vertex_intensity(bx,by,bz));
        uint32_t cc = pal_intensity_color(dng_fog_vertex_intensity(cx,cy,cz));
        uint32_t cd = pal_intensity_color(dng_fog_vertex_intensity(dx,dy,dz));
        sr_draw_quad_indexed(fb_ptr,
            sr_vert_c(ax,ay,az, u0,v0, ca),
            sr_vert_c(bx,by,bz, u1,v0, cb),
            sr_vert_c(cx,cy,cz, u1,v1, cc),
            sr_vert_c(dx,dy,dz, u0,v1, cd),
            tex, mvp);
    }
}

/* Draw the dungeon scene */
static void draw_dungeon_scene(sr_framebuffer *fb_ptr, const sr_mat4 *vp) {
    sr_dungeon *d = dng_state.dungeon;
    dng_player *p = &dng_state.player;

    /* Set pixel-light callback for torch mode */
    if (dng_light_mode == 0)
        sr_set_pixel_light_fn(dng_torch_light);

    float cam_x = p->x;
    float cam_y = p->y;
    float cam_z = p->z;
    float cam_angle = p->angle * 6.28318f; /* turns to radians */

    /* Build camera matrix: first-person view */
    float ca_cos = cosf(cam_angle), ca_sin = sinf(cam_angle);
    /* Forward direction = (sin(angle), 0, -cos(angle))
     * Note: Picotron sin() is negated vs C sinf(), so we use +sin here */
    sr_vec3 eye = { cam_x, cam_y, cam_z };
    sr_vec3 fwd = { ca_sin, 0, -ca_cos };
    sr_vec3 target = { eye.x + fwd.x, eye.y + fwd.y, eye.z + fwd.z };
    sr_vec3 up = { 0, 1, 0 };

    sr_mat4 view = sr_mat4_lookat(eye, target, up);
    sr_mat4 proj = sr_mat4_perspective(
        70.0f * 3.14159f / 180.0f,
        (float)FB_WIDTH / (float)FB_HEIGHT,
        0.05f, 40.0f
    );
    sr_mat4 mvp = sr_mat4_mul(proj, view);

    /* Build visibility */
    dng_build_visibility(p, d);

    float y_lo = -DNG_HALF_CELL;
    float y_hi = DNG_HALF_CELL;
    float P = DNG_PILLAR_PAD;

    int pgx = p->gx, pgy = p->gy;
    int gx0 = pgx - DNG_RENDER_R; if (gx0 < 1) gx0 = 1;
    int gx1 = pgx + DNG_RENDER_R; if (gx1 > d->w) gx1 = d->w;
    int gy0 = pgy - DNG_RENDER_R; if (gy0 < 1) gy0 = 1;
    int gy1 = pgy + DNG_RENDER_R; if (gy1 > d->h) gy1 = d->h;

    /* Render cells */
    for (int gy = gy0; gy <= gy1; gy++) {
        for (int gx = gx0; gx <= gx1; gx++) {
            if (!dng_vis[gy][gx]) continue;

            float x0 = (gx - 1) * DNG_CELL_SIZE;
            float x1 = gx * DNG_CELL_SIZE;
            float z0 = (gy - 1) * DNG_CELL_SIZE;
            float z1 = gy * DNG_CELL_SIZE;

            if (d->map[gy][gx] == 1) {
                /* Wall cell — draw faces toward open cells */
                /* South face (normal = 0,0,+1) */
                if (gy < d->h && d->map[gy+1][gx] != 1 && dng_vis[gy+1][gx]) {
                    dng_draw_wall(fb_ptr, &mvp,
                        x0+P, y_hi, z1,  x1-P, y_hi, z1,
                        x1-P, y_lo, z1,  x0+P, y_lo, z1,
                        &itextures[ITEX_BRICK], 0, 0, 1);
                }
                /* North face (normal = 0,0,-1) */
                if (gy > 1 && d->map[gy-1][gx] != 1 && dng_vis[gy-1][gx]) {
                    dng_draw_wall(fb_ptr, &mvp,
                        x1-P, y_hi, z0,  x0+P, y_hi, z0,
                        x0+P, y_lo, z0,  x1-P, y_lo, z0,
                        &itextures[ITEX_BRICK], 0, 0, -1);
                }
                /* East face (normal = +1,0,0) */
                if (gx < d->w && d->map[gy][gx+1] != 1 && dng_vis[gy][gx+1]) {
                    dng_draw_wall(fb_ptr, &mvp,
                        x1, y_hi, z1-P,  x1, y_hi, z0+P,
                        x1, y_lo, z0+P,  x1, y_lo, z1-P,
                        &itextures[ITEX_BRICK], 1, 0, 0);
                }
                /* West face (normal = -1,0,0) */
                if (gx > 1 && d->map[gy][gx-1] != 1 && dng_vis[gy][gx-1]) {
                    dng_draw_wall(fb_ptr, &mvp,
                        x0, y_hi, z0+P,  x0, y_hi, z1-P,
                        x0, y_lo, z1-P,  x0, y_lo, z0+P,
                        &itextures[ITEX_BRICK], -1, 0, 0);
                }
            } else {
                /* Open cell */
                bool is_up_stairs = (d->has_up && gx == d->stairs_gx && gy == d->stairs_gy);
                bool is_down_stairs = (d->has_down && gx == d->down_gx && gy == d->down_gy);

                if (is_up_stairs || is_down_stairs) {
                    /* Draw stairs */
                    int sdir = is_up_stairs ? d->stairs_dir : d->down_dir;
                    bool going_down = is_down_stairs;
                    float y_range = y_hi - y_lo;
                    float step_h = y_range / DNG_NUM_STEPS;
                    float step_d = DNG_CELL_SIZE / DNG_NUM_STEPS;

                    /* Riser normal directions per stair direction */
                    static const float riser_nx[4] = { 0, -1, 0,  1};
                    static const float riser_nz[4] = { 1,  0,-1,  0};

                    const sr_indexed_texture *stex = &itextures[ITEX_STONE];

                    for (int i = 0; i < DNG_NUM_STEPS; i++) {
                        float tread_y, riser_top, riser_bot;
                        float side_top, side_bot; /* full-height side walls */
                        if (going_down) {
                            tread_y = y_lo - (i + 1) * step_h;
                            riser_top = y_lo - i * step_h;
                            riser_bot = tread_y;
                            side_top = y_lo;
                            side_bot = tread_y;
                        } else {
                            tread_y = y_lo + (i + 1) * step_h;
                            riser_top = tread_y;
                            riser_bot = y_lo + i * step_h;
                            side_top = tread_y;
                            side_bot = y_lo;
                        }

                        float sx0, sx1, sz0, sz1;
                        if (sdir == 0) { /* North */
                            sz0 = z1 - (i + 1) * step_d;
                            sz1 = z1 - i * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                x0,tread_y,sz0, x1,tread_y,sz0,
                                x1,tread_y,sz1, x0,tread_y,sz1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                x0,riser_top,sz1, x1,riser_top,sz1,
                                x1,riser_bot,sz1, x0,riser_bot,sz1,
                                stex, riser_nx[0],0,riser_nz[0]);
                            /* Side walls (west & east) */
                            dng_draw_wall(fb_ptr, &mvp,
                                x0,side_top,sz0, x0,side_top,sz1,
                                x0,side_bot,sz1, x0,side_bot,sz0,
                                stex, -1,0,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                x1,side_top,sz1, x1,side_top,sz0,
                                x1,side_bot,sz0, x1,side_bot,sz1,
                                stex, 1,0,0);
                        } else if (sdir == 1) { /* East */
                            sx0 = x0 + i * step_d;
                            sx1 = x0 + (i + 1) * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                sx0,tread_y,z0, sx1,tread_y,z0,
                                sx1,tread_y,z1, sx0,tread_y,z1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                sx0,riser_top,z0, sx0,riser_top,z1,
                                sx0,riser_bot,z1, sx0,riser_bot,z0,
                                stex, riser_nx[1],0,riser_nz[1]);
                            /* Side walls (north & south) */
                            dng_draw_wall(fb_ptr, &mvp,
                                sx0,side_top,z0, sx1,side_top,z0,
                                sx1,side_bot,z0, sx0,side_bot,z0,
                                stex, 0,0,-1);
                            dng_draw_wall(fb_ptr, &mvp,
                                sx1,side_top,z1, sx0,side_top,z1,
                                sx0,side_bot,z1, sx1,side_bot,z1,
                                stex, 0,0,1);
                        } else if (sdir == 2) { /* South */
                            sz0 = z0 + i * step_d;
                            sz1 = z0 + (i + 1) * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                x0,tread_y,sz0, x1,tread_y,sz0,
                                x1,tread_y,sz1, x0,tread_y,sz1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                x1,riser_top,sz0, x0,riser_top,sz0,
                                x0,riser_bot,sz0, x1,riser_bot,sz0,
                                stex, riser_nx[2],0,riser_nz[2]);
                            /* Side walls (west & east) */
                            dng_draw_wall(fb_ptr, &mvp,
                                x0,side_top,sz1, x0,side_top,sz0,
                                x0,side_bot,sz0, x0,side_bot,sz1,
                                stex, -1,0,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                x1,side_top,sz0, x1,side_top,sz1,
                                x1,side_bot,sz1, x1,side_bot,sz0,
                                stex, 1,0,0);
                        } else { /* West (3) */
                            sx0 = x1 - (i + 1) * step_d;
                            sx1 = x1 - i * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                sx0,tread_y,z0, sx1,tread_y,z0,
                                sx1,tread_y,z1, sx0,tread_y,z1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                sx1,riser_top,z1, sx1,riser_top,z0,
                                sx1,riser_bot,z0, sx1,riser_bot,z1,
                                stex, riser_nx[3],0,riser_nz[3]);
                            /* Side walls (north & south) */
                            dng_draw_wall(fb_ptr, &mvp,
                                sx1,side_top,z0, sx0,side_top,z0,
                                sx0,side_bot,z0, sx1,side_bot,z0,
                                stex, 0,0,-1);
                            dng_draw_wall(fb_ptr, &mvp,
                                sx0,side_top,z1, sx1,side_top,z1,
                                sx1,side_bot,z1, sx0,side_bot,z1,
                                stex, 0,0,1);
                        }
                    }

                    /* Down-stairs also get a ceiling */
                    if (is_down_stairs) {
                        dng_draw_hquad(fb_ptr, &mvp,
                            x0,y_hi,z1, x1,y_hi,z1,
                            x1,y_hi,z0, x0,y_hi,z0,
                            0,1,1,0, &itextures[ITEX_WOOD], 0,-1,0);
                    }
                } else {
                    /* Normal floor + ceiling */
                    dng_draw_hquad(fb_ptr, &mvp,
                        x0,y_lo,z0, x1,y_lo,z0,
                        x1,y_lo,z1, x0,y_lo,z1,
                        0,0,1,1, &itextures[ITEX_TILE], 0,1,0);

                    dng_draw_hquad(fb_ptr, &mvp,
                        x0,y_hi,z1, x1,y_hi,z1,
                        x1,y_hi,z0, x0,y_hi,z0,
                        0,1,1,0, &itextures[ITEX_WOOD], 0,-1,0);
                }
            }
        }
    }

    /* Pillars at grid intersections */
    for (int vy = gy0; vy <= gy1 + 1; vy++) {
        for (int vx = gx0; vx <= gx1 + 1; vx++) {
            bool nw_open = vx > 1 && vy > 1 && d->map[vy-1][vx-1] != 1;
            bool ne_open = vx <= d->w && vy > 1 && d->map[vy-1][vx] != 1;
            bool sw_open = vx > 1 && vy <= d->h && d->map[vy][vx-1] != 1;
            bool se_open = vx <= d->w && vy <= d->h && d->map[vy][vx] != 1;

            bool has_open = nw_open || ne_open || sw_open || se_open;
            bool all_open = nw_open && ne_open && sw_open && se_open;

            if (has_open && !all_open) {
                /* Check if any adjacent open cell is visible */
                bool visible = false;
                if (nw_open && vx-1 >= 1 && vy-1 >= 1 && dng_vis[vy-1][vx-1]) visible = true;
                if (ne_open && vx <= d->w && vy-1 >= 1 && dng_vis[vy-1][vx]) visible = true;
                if (sw_open && vx-1 >= 1 && vy <= d->h && dng_vis[vy][vx-1]) visible = true;
                if (se_open && vx <= d->w && vy <= d->h && dng_vis[vy][vx]) visible = true;
                if (!visible) continue;

                float wx = (vx - 1) * DNG_CELL_SIZE;
                float wz = (vy - 1) * DNG_CELL_SIZE;

                /* South face */
                if (sw_open || se_open) {
                    float fx0 = sw_open ? wx - P : wx;
                    float fx1 = se_open ? wx + P : wx;
                    if (fx0 < fx1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            fx0, y_hi, wz+P,  fx1, y_hi, wz+P,
                            fx1, y_lo, wz+P,  fx0, y_lo, wz+P,
                            &itextures[ITEX_BRICK], 0, 0, 1);
                    }
                }
                /* North face */
                if (nw_open || ne_open) {
                    float fx0 = nw_open ? wx - P : wx;
                    float fx1 = ne_open ? wx + P : wx;
                    if (fx0 < fx1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            fx1, y_hi, wz-P,  fx0, y_hi, wz-P,
                            fx0, y_lo, wz-P,  fx1, y_lo, wz-P,
                            &itextures[ITEX_BRICK], 0, 0, -1);
                    }
                }
                /* East face */
                if (ne_open || se_open) {
                    float fz0 = ne_open ? wz - P : wz;
                    float fz1 = se_open ? wz + P : wz;
                    if (fz0 < fz1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            wx+P, y_hi, fz1,  wx+P, y_hi, fz0,
                            wx+P, y_lo, fz0,  wx+P, y_lo, fz1,
                            &itextures[ITEX_BRICK], 1, 0, 0);
                    }
                }
                /* West face */
                if (nw_open || sw_open) {
                    float fz0 = nw_open ? wz - P : wz;
                    float fz1 = sw_open ? wz + P : wz;
                    if (fz0 < fz1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            wx-P, y_hi, fz0,  wx-P, y_hi, fz1,
                            wx-P, y_lo, fz1,  wx-P, y_lo, fz0,
                            &itextures[ITEX_BRICK], -1, 0, 0);
                    }
                }
            }
        }
    }
}

/* Draw dungeon minimap */
static void minimap_pixel(uint32_t *px, int rx, int ry, uint32_t col) {
    if (rx >= 0 && rx < FB_WIDTH && ry >= 0 && ry < FB_HEIGHT)
        px[ry * FB_WIDTH + rx] = col;
}

/* Draw a line on the minimap using Bresenham */
static void minimap_line(uint32_t *px, int x0, int y0, int x1, int y1, uint32_t col) {
    int ddx = x1 - x0; if (ddx < 0) ddx = -ddx;
    int ddy = y1 - y0; if (ddy < 0) ddy = -ddy;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = ddx - ddy;
    for (;;) {
        minimap_pixel(px, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -ddy) { err -= ddy; x0 += sx; }
        if (e2 <  ddx) { err += ddx; y0 += sy; }
    }
}

static void draw_dungeon_minimap(sr_framebuffer *fb_ptr) {
    sr_dungeon *d = dng_state.dungeon;
    dng_player *p = &dng_state.player;
    int scale = 2;
    int mx = FB_WIDTH - d->w * scale - 4;
    int my = 4;
    uint32_t *px = fb_ptr->color;

    /* Draw all open (floor) cells */
    for (int y = 1; y <= d->h; y++) {
        for (int x = 1; x <= d->w; x++) {
            if (d->map[y][x] == 1) continue;
            int px0 = mx + (x - 1) * scale;
            int py0 = my + (y - 1) * scale;
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++)
                    minimap_pixel(px, px0 + dx, py0 + dy, 0xFF444444);
        }
    }

    /* Stairs markers */
    if (d->has_up) {
        int sx = d->stairs_gx, sy = d->stairs_gy;
        int px0 = mx + (sx - 1) * scale, py0 = my + (sy - 1) * scale;
        for (int dy = 0; dy < scale; dy++)
            for (int dx = 0; dx < scale; dx++)
                minimap_pixel(px, px0 + dx, py0 + dy, 0xFF00CC00);
    }
    if (d->has_down) {
        int sx = d->down_gx, sy = d->down_gy;
        int px0 = mx + (sx - 1) * scale, py0 = my + (sy - 1) * scale;
        for (int dy = 0; dy < scale; dy++)
            for (int dx = 0; dx < scale; dx++)
                minimap_pixel(px, px0 + dx, py0 + dy, 0xFF0000CC);
    }

    /* Player dot (yellow) */
    float pcx = mx + (p->gx - 1) * scale + scale * 0.5f;
    float pcy = my + (p->gy - 1) * scale + scale * 0.5f;
    for (int dy = 0; dy < scale; dy++)
        for (int dx = 0; dx < scale; dx++)
            minimap_pixel(px, (int)pcx - scale/2 + dx, (int)pcy - scale/2 + dy, 0xFF00FFFF);

    /* View cone — two lines from player in facing direction, ~60° FOV each side */
    float cone_len = 6.0f * scale; /* length in minimap pixels */
    /* Player facing angle: dir 0=N(-Z), 1=E(+X), 2=S(+Z), 3=W(-X)
     * On minimap: +X = right, +Y = down. N=-Y, E=+X, S=+Y, W=-X */
    float face_angle = p->angle * 2.0f * 3.14159265f; /* angle is in full turns (0.25 = 90°) */
    /* Map angle: 0=N(up), pi/2=E(right), pi=S(down), 3pi/2=W(left)
     * On screen: up=-Y, right=+X → screen_x = sin(a), screen_y = -cos(a)
     * But dir 0=N means facing -Z in world, which is UP on minimap */
    float fwd_sx = sinf(face_angle);
    float fwd_sy = -cosf(face_angle);

    float half_fov = 0.52f; /* ~30° half-angle */
    float cos_hf = cosf(half_fov), sin_hf = sinf(half_fov);

    /* Left edge of cone */
    float lx = fwd_sx * cos_hf - fwd_sy * sin_hf;
    float ly = fwd_sx * sin_hf + fwd_sy * cos_hf;
    /* Right edge of cone */
    float rx = fwd_sx * cos_hf + fwd_sy * sin_hf;
    float ry = -fwd_sx * sin_hf + fwd_sy * cos_hf;

    int lx1 = (int)(pcx + lx * cone_len);
    int ly1 = (int)(pcy + ly * cone_len);
    int rx1 = (int)(pcx + rx * cone_len);
    int ry1 = (int)(pcy + ry * cone_len);

    uint32_t cone_col = 0xFF00CCCC; /* dim yellow */
    minimap_line(px, (int)pcx, (int)pcy, lx1, ly1, cone_col);
    minimap_line(px, (int)pcx, (int)pcy, rx1, ry1, cone_col);
}

/* ── Stats overlay ───────────────────────────────────────────────── */

static void draw_stats(sr_framebuffer *fb_ptr, int tris) {
    char buf[64];
    uint32_t white  = 0xFFFFFFFF;
    uint32_t shadow = 0xFF000000;

    if (current_scene != SCENE_DUNGEON) {
        /* FPS */
        snprintf(buf, sizeof(buf), "FPS: %d", fps_display);
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            3, 3, buf, white, shadow);

        /* Triangle count */
        snprintf(buf, sizeof(buf), "TRIS: %d", tris);
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            3, 13, buf, white, shadow);

        /* Resolution + scene name */
        snprintf(buf, sizeof(buf), "%dX%d  %s", FB_WIDTH, FB_HEIGHT,
                 scene_names[current_scene]);
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            3, 23, buf, white, shadow);

        /* MENU button (top-right corner) */
        {
            int mbx = FB_WIDTH - 32, mby = 3, mbw = 30, mbh = 11;
            uint32_t *px = fb_ptr->color;
            for (int ry = mby; ry < mby + mbh && ry < FB_HEIGHT; ry++)
                for (int rx = mbx; rx < mbx + mbw && rx < FB_WIDTH; rx++)
                    if (ry >= 0 && rx >= 0) px[ry * FB_WIDTH + rx] = 0xC0000000;
            sr_draw_text_shadow(px, fb_ptr->width, fb_ptr->height,
                                mbx + 3, mby + 2, "MENU", 0xFF999999, shadow);
        }
    }

    /* Recording indicator (always visible) */
    if (sr_gif_is_recording()) {
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            FB_WIDTH - 30, 3, "REC", 0xFF0000FF, shadow);
    }

    /* Night mode hint (neighborhood only) */
    if (current_scene == SCENE_NEIGHBORHOOD) {
        const char *ntxt = night_mode ? "N = DAY" : "N = NIGHT";
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            3, FB_HEIGHT - 12, ntxt, 0xFF999999, shadow);
    }

    /* Palette scene lighting controls — clickable buttons */
    if (current_scene == SCENE_PALETTE_HOUSE) {
        uint32_t sel_col  = 0xFF55CCFF;
        uint32_t dim_col  = 0xFF999999;
        uint32_t btn_bg   = 0xC0000000;  /* semi-transparent black */
        uint32_t btn_hi   = 0xC0332200;  /* highlighted button bg */
        int W = fb_ptr->width, H = fb_ptr->height;
        uint32_t *px = fb_ptr->color;

        /* Helper: draw filled rect */
        #define DRAW_RECT(x0,y0,x1,y1,col) do { \
            for (int ry = (y0); ry < (y1) && ry < H; ry++) \
                for (int rx = (x0); rx < (x1) && rx < W; rx++) \
                    if (ry >= 0 && rx >= 0) px[ry * W + rx] = (col); \
        } while(0)

        /* Button layout: left column at x=2, each button 10px tall, 2px gap */
        int bx = 2, bw = 100;
        int by0 = H - 62;  /* AMB row */
        int by1 = H - 50;  /* LIGHT row */
        int by2 = H - 38;  /* SHADING row */
        int by3 = H - 26;  /* SHADOWS row */
        int bh = 11;

        /* AMB: value [-] [+] */
        DRAW_RECT(bx, by0, bx + bw, by0 + bh, adjusting_ambient ? btn_hi : btn_bg);
        snprintf(buf, sizeof(buf), "AMB: %.2f", pal_ambient);
        sr_draw_text_shadow(px, W, H, bx + 2, by0 + 2, buf,
                            adjusting_ambient ? sel_col : dim_col, shadow);
        /* [-] button */
        DRAW_RECT(bx + bw + 2, by0, bx + bw + 14, by0 + bh, btn_bg);
        sr_draw_text_shadow(px, W, H, bx + bw + 4, by0 + 2, "-", white, shadow);
        /* [+] button */
        DRAW_RECT(bx + bw + 16, by0, bx + bw + 28, by0 + bh, btn_bg);
        sr_draw_text_shadow(px, W, H, bx + bw + 18, by0 + 2, "+", white, shadow);

        /* LIGHT: value [-] [+] */
        DRAW_RECT(bx, by1, bx + bw, by1 + bh, !adjusting_ambient ? btn_hi : btn_bg);
        snprintf(buf, sizeof(buf), "LIGHT: %.1fx", pal_light_mult);
        sr_draw_text_shadow(px, W, H, bx + 2, by1 + 2, buf,
                            !adjusting_ambient ? sel_col : dim_col, shadow);
        DRAW_RECT(bx + bw + 2, by1, bx + bw + 14, by1 + bh, btn_bg);
        sr_draw_text_shadow(px, W, H, bx + bw + 4, by1 + 2, "-", white, shadow);
        DRAW_RECT(bx + bw + 16, by1, bx + bw + 28, by1 + bh, btn_bg);
        sr_draw_text_shadow(px, W, H, bx + bw + 18, by1 + 2, "+", white, shadow);

        /* SHADING toggle button */
        int sw = pixel_lighting ? 84 : 90;
        DRAW_RECT(bx, by2, bx + sw, by2 + bh, pixel_lighting ? btn_hi : btn_bg);
        snprintf(buf, sizeof(buf), "SHADING: %s", pixel_lighting ? "PIXEL" : "VERTEX");
        sr_draw_text_shadow(px, W, H, bx + 2, by2 + 2, buf,
                            pixel_lighting ? sel_col : dim_col, shadow);

        /* SHADOWS toggle button */
        int shw = shadows_enabled ? 72 : 78;
        DRAW_RECT(bx, by3, bx + shw, by3 + bh, shadows_enabled ? btn_hi : btn_bg);
        snprintf(buf, sizeof(buf), "SHADOWS: %s", shadows_enabled ? "ON" : "OFF");
        sr_draw_text_shadow(px, W, H, bx + 2, by3 + 2, buf,
                            shadows_enabled ? sel_col : dim_col, shadow);

        #undef DRAW_RECT
    }
}

/* ── Menu overlay ────────────────────────────────────────────────── */

static void draw_menu(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    (void)H;

    /* Darken framebuffer */
    for (int i = 0; i < W * H; i++) {
        uint32_t c = px[i];
        uint8_t r = ((c      ) & 0xFF) >> 2;
        uint8_t g = ((c >>  8) & 0xFF) >> 2;
        uint8_t b = ((c >> 16) & 0xFF) >> 2;
        uint8_t a = (c >> 24) & 0xFF;
        px[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }

    uint32_t white  = 0xFFFFFFFF;
    uint32_t gray   = 0xFF999999;
    uint32_t yellow = 0xFF00FFFF;
    uint32_t shadow = 0xFF000000;

    sr_draw_text_shadow(px, W, H, 180, 50, "STARRASTER", white, shadow);
    sr_draw_text_shadow(px, W, H, 150, 90, "SELECT SCENE:", gray, shadow);

    for (int i = 0; i < SCENE_COUNT; i++) {
        char line[64];
        snprintf(line, sizeof(line), "[%d]  %s", i + 1, scene_names[i]);
        uint32_t color = (i == menu_cursor) ? yellow : white;
        sr_draw_text_shadow(px, W, H, 150, 115 + i * 15, line, color, shadow);
    }

    sr_draw_text_shadow(px, W, H, 130, 180, "UP/DOWN  SELECT", gray, shadow);
    sr_draw_text_shadow(px, W, H, 130, 195, "ENTER    START", gray, shadow);
    sr_draw_text_shadow(px, W, H, 130, 210, "TAB/ESC  MENU", gray, shadow);
    sr_draw_text_shadow(px, W, H, 130, 225, "TAP      SELECT/BACK", gray, shadow);
}

/* ── Frame limiter ───────────────────────────────────────────────── */

#if defined(_WIN32)
static void frame_limiter(void) {
    static LARGE_INTEGER freq = {0}, last = {0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);
    }
    double target = 1.0 / (double)TARGET_FPS;
    for (;;) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double elapsed = (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart;
        if (elapsed >= target) { last = now; return; }
        if (target - elapsed > 0.002) Sleep(1);
    }
}
#elif !defined(__EMSCRIPTEN__)
static void frame_limiter(void) {
    static struct timespec last = {0};
    if (last.tv_sec == 0 && last.tv_nsec == 0) clock_gettime(CLOCK_MONOTONIC, &last);
    double target = 1.0 / (double)TARGET_FPS;
    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) * 1e-9;
        if (elapsed >= target) { last = now; return; }
        usleep(500);
    }
}
#else
static void frame_limiter(void) { /* WASM: requestAnimationFrame handles this */ }
#endif

/* ── PNG screenshot ──────────────────────────────────────────────── */

#include "../third_party/stb/stb_image_write.h"

static void save_screenshot(const uint32_t *pixels, int w, int h) {
#ifdef _WIN32
    _mkdir("screenshots");
#else
    mkdir("screenshots", 0755);
#endif

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[256];
    snprintf(filename, sizeof(filename),
             "screenshots/screenshot_%04d%02d%02d_%02d%02d%02d_%d.png",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec, screenshot_counter++);

    /* Convert XRGB to RGB for stbi_write_png */
    uint8_t *rgb = (uint8_t *)malloc(w * h * 3);
    if (!rgb) { fprintf(stderr, "[Screenshot] Out of memory\n"); return; }

    for (int i = 0; i < w * h; i++) {
        uint32_t c = pixels[i];
        rgb[i * 3 + 0] = (uint8_t)((c      ) & 0xFF);  /* R */
        rgb[i * 3 + 1] = (uint8_t)((c >>  8) & 0xFF);  /* G */
        rgb[i * 3 + 2] = (uint8_t)((c >> 16) & 0xFF);  /* B */
    }

    if (!stbi_write_png(filename, w, h, 3, rgb, w * 3))
        fprintf(stderr, "[Screenshot] Failed to save %s\n", filename);
    else
        printf("[Screenshot] Saved %s\n", filename);

    free(rgb);
}

/* ── Sokol callbacks ─────────────────────────────────────────────── */

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    fb = sr_framebuffer_create(FB_WIDTH, FB_HEIGHT);
    shadow_fb = sr_framebuffer_create(SHADOW_SIZE, SHADOW_SIZE);

    fb_image = sg_make_image(&(sg_image_desc){
        .width  = FB_WIDTH,
        .height = FB_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage.stream_update = true,
    });

    fb_view = sg_make_view(&(sg_view_desc){
        .texture.image = fb_image,
    });

    fb_sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });

    float verts[] = {
        -1, -1,  0, 1,
         1, -1,  1, 1,
         1,  1,  1, 0,
        -1, -1,  0, 1,
         1,  1,  1, 0,
        -1,  1,  0, 0,
    };
    vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(verts),
    });

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source   = vs_src,
        .fragment_func.source = fs_src,
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(float) * 2,
            .glsl_uniforms[0] = { .glsl_name = "u_scale", .type = SG_UNIFORMTYPE_FLOAT2 },
        },
        .views[0].texture = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .image_type = SG_IMAGETYPE_2D,
            .sample_type = SG_IMAGESAMPLETYPE_FLOAT,
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .sampler_type = SG_SAMPLERTYPE_FILTERING,
        },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .glsl_name = "tex",
            .view_slot = 0,
            .sampler_slot = 0,
        },
    });

    pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT2 },
                [1] = { .format = SG_VERTEXFORMAT_FLOAT2 },
            },
        },
    });

    bind.vertex_buffers[0] = vbuf;
    bind.views[0]     = fb_view;
    bind.samplers[0]  = fb_sampler;

    textures[TEX_BRICK] = sr_texture_load("assets/bricks.png");
    textures[TEX_GRASS] = sr_texture_load("assets/grass.png");
    textures[TEX_ROOF]  = sr_texture_load("assets/roof.png");
    textures[TEX_WOOD]  = sr_texture_load("assets/wood.png");
    textures[TEX_TILE]  = sr_texture_load("assets/tile.png");

    /* Indexed (palette) textures */
    itextures[ITEX_BRICK] = sr_indexed_load("assets/indexed/bricks.idx");
    itextures[ITEX_GRASS] = sr_indexed_load("assets/indexed/grass.idx");
    itextures[ITEX_ROOF]  = sr_indexed_load("assets/indexed/roof.idx");
    itextures[ITEX_WOOD]  = sr_indexed_load("assets/indexed/wood.idx");
    itextures[ITEX_TILE]  = sr_indexed_load("assets/indexed/tile.idx");
    itextures[ITEX_STONE] = sr_indexed_load("assets/indexed/stone.idx");

    /* Dithered distance fog — matches sky color */
    sr_fog_set(FOG_COLOR, FOG_NEAR, FOG_FAR);

    /* Pre-init cube scene data */
    cubes_scene_init();

    /* Pre-init dungeon */
    dng_load_config();
    dng_game_init(&dng_state);
    dng_initialized = true;

#ifdef _WIN32
    timeBeginPeriod(1);  /* 1ms Sleep resolution for frame limiter */
#endif

    printf("StarRaster initialized (%dx%d @ %dfps)\n", FB_WIDTH, FB_HEIGHT, TARGET_FPS);
    printf("  TAB    = menu\n");
    printf("  Ctrl+8 = start GIF recording (24fps)\n");
    printf("  Ctrl+9 = stop & save GIF\n");
    printf("  Ctrl+P = save screenshot\n");
    printf("  ESC    = quit\n");
}

static void frame(void) {
    if (frame_counter == 0) printf("[DBG] frame() start\n");
    double dt = sapp_frame_duration();
    time_acc += dt;
    frame_counter++;

    /* FPS counter */
    fps_timer += dt;
    fps_frame_count++;
    if (fps_timer >= 1.0) {
        fps_display = fps_frame_count;
        fps_frame_count = 0;
        fps_timer -= 1.0;
    }

    /* ── Camera ──────────────────────────────────────────────── */
    float angle = (float)time_acc * 0.25f;
    float cam_dist, cam_height;
    sr_vec3 target_pos;

    if (current_scene == SCENE_CUBES) {
        cam_dist   = 45.0f;
        cam_height = 20.0f;
        target_pos = (sr_vec3){ 0, 5.0f, 0 };
    } else if (current_scene == SCENE_PALETTE_HOUSE) {
        cam_dist   = 10.0f;
        cam_height = 5.0f;
        target_pos = (sr_vec3){ 0, 1.5f, 0 };
    } else {
        cam_dist   = 28.0f;
        cam_height = 12.0f;
        target_pos = (sr_vec3){ 0, 1.5f, 0 };
    }

    sr_vec3 eye = {
        cosf(angle) * cam_dist,
        cam_height,
        sinf(angle) * cam_dist
    };
    sr_vec3 up = { 0, 1, 0 };

    sr_mat4 view = sr_mat4_lookat(eye, target_pos, up);
    sr_mat4 proj = sr_mat4_perspective(
        60.0f * 3.14159f / 180.0f,
        (float)FB_WIDTH / (float)FB_HEIGHT,
        0.1f, 100.0f
    );
    sr_mat4 vp = sr_mat4_mul(proj, view);

    /* ── CPU rasterize ───────────────────────────────────────── */
    sr_stats_reset();
    uint32_t clear_color;
    if (current_scene == SCENE_PALETTE_HOUSE)
        clear_color = PAL_NIGHT_SKY;
    else if (current_scene == SCENE_DUNGEON)
        clear_color = 0xFF000000;  /* black for dungeon */
    else if (night_mode && current_scene == SCENE_NEIGHBORHOOD)
        clear_color = NIGHT_SKY_COLOR;
    else
        clear_color = FOG_COLOR;
    sr_framebuffer_clear(&fb, clear_color, 1.0f);
    if (frame_counter <= 1) printf("[DBG] fb cleared, state=%d scene=%d\n", app_state, current_scene);

    if (app_state == STATE_RUNNING) {
        switch (current_scene) {
            case SCENE_NEIGHBORHOOD:
                setup_street_lights((float)time_acc);
                build_light_grid();
                draw_neighborhood(&fb, &vp);
                break;
            case SCENE_CUBES:
                draw_cube_scene(&fb, &vp, (float)time_acc);
                break;
            case SCENE_PALETTE_HOUSE:
                sr_fog_disable();
                draw_palette_scene(&fb, &vp, (float)time_acc);
                break;
            case SCENE_DUNGEON:
                sr_fog_disable();
                /* Update dungeon game state */
                if (dng_play_state == DNG_STATE_CLIMBING) {
                    if (dng_update_climb(&dng_state)) {
                        dng_play_state = DNG_STATE_PLAYING;
                    }
                } else {
                    /* Check stairs */
                    sr_dungeon *dd = dng_state.dungeon;
                    dng_player *pp = &dng_state.player;
                    bool on_up = (dd->has_up && pp->gx == dd->stairs_gx && pp->gy == dd->stairs_gy);
                    bool on_down = (dd->has_down && pp->gx == dd->down_gx && pp->gy == dd->down_gy);
                    if (dng_state.on_stairs) {
                        if (!on_up && !on_down) dng_state.on_stairs = false;
                    } else {
                        if (on_up) {
                            dng_start_climb(&dng_state, true);
                            dng_play_state = DNG_STATE_CLIMBING;
                        } else if (on_down) {
                            dng_start_climb(&dng_state, false);
                            dng_play_state = DNG_STATE_CLIMBING;
                        }
                    }
                }
                dng_player_update(&dng_state.player);
                draw_dungeon_scene(&fb, &vp);
                draw_dungeon_minimap(&fb);
                break;
        }
    }

    int tris = sr_stats_tri_count();
    if (frame_counter <= 1) printf("[DBG] pre draw_stats\n");
    draw_stats(&fb, tris);
    if (frame_counter <= 1) printf("[DBG] post draw_stats\n");

    if (app_state == STATE_MENU)
        draw_menu(&fb);
    if (frame_counter <= 1) printf("[DBG] post draw_menu\n");

    /* ── GIF capture (time-based, 24fps) ─────────────────────── */
    if (sr_gif_is_recording()) {
        gif_capture_timer += dt;
        double interval = 1.0 / GIF_TARGET_FPS;
        while (gif_capture_timer >= interval) {
            gif_capture_timer -= interval;
            sr_gif_capture_frame(fb.color);
        }
    }

    /* ── Aspect-ratio-preserving scale ───────────────────────── */
    float fb_aspect  = (float)FB_WIDTH / (float)FB_HEIGHT;
    float win_aspect = sapp_widthf() / sapp_heightf();
    float scale[2];
    if (win_aspect > fb_aspect) {
        scale[0] = fb_aspect / win_aspect;
        scale[1] = 1.0f;
    } else {
        scale[0] = 1.0f;
        scale[1] = win_aspect / fb_aspect;
    }

    /* ── Upload and display ──────────────────────────────────── */
    if (frame_counter <= 1) printf("[DBG] pre sg_update_image\n");
    sg_update_image(fb_image, &(sg_image_data){
        .mip_levels[0] = { .ptr = fb.color, .size = FB_WIDTH * FB_HEIGHT * 4 }
    });
    if (frame_counter <= 1) printf("[DBG] post sg_update_image\n");

    sg_begin_pass(&(sg_pass){
        .action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.05f, 0.05f, 0.08f, 1.0f },
            },
        },
        .swapchain = sglue_swapchain(),
    });
    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
    sg_apply_uniforms(0, &(sg_range){ &scale, sizeof(scale) });
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();

    frame_limiter();
}

static void cleanup(void) {
    if (sr_gif_is_recording())
        sr_gif_stop_and_save();
    for (int i = 0; i < TEX_COUNT; i++)
        sr_texture_free(&textures[i]);
    for (int i = 0; i < ITEX_COUNT; i++)
        sr_indexed_free(&itextures[i]);
    sr_framebuffer_destroy(&fb);
    sr_framebuffer_destroy(&shadow_fb);
    sg_shutdown();
#ifdef _WIN32
    timeEndPeriod(1);
#endif
}

/* Map window coordinates to framebuffer pixel coordinates */
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

/* Handle a tap/click at screen coordinates */
static void handle_tap(float sx, float sy) {
    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);

    if (app_state == STATE_MENU) {
        /* Menu items are at x=150, y=115 + i*15, text is ~120px wide */
        for (int i = 0; i < SCENE_COUNT; i++) {
            float item_y = 115.0f + (float)i * 15.0f;
            if (fx >= 140.0f && fx <= 340.0f &&
                fy >= item_y - 4.0f && fy <= item_y + 12.0f) {
                current_scene = i;
                menu_cursor = i;
                app_state = STATE_RUNNING;
                return;
            }
        }
    } else {
        /* MENU button check (all scenes, top-right) */
        int mbx = FB_WIDTH - 32, mby = 3, mbw = 30, mbh = 11;
        if (fx >= mbx && fx <= mbx + mbw && fy >= mby && fy <= mby + mbh) {
            app_state = STATE_MENU;
            return;
        }

        if (current_scene == SCENE_PALETTE_HOUSE) {
        /* Palette scene button hit testing */
        int bx = 2, bw = 100, bh = 11;
        int by0 = FB_HEIGHT - 62;   /* AMB row */
        int by1 = FB_HEIGHT - 50;   /* LIGHT row */
        int by2 = FB_HEIGHT - 38;   /* SHADING row */
        int by3 = FB_HEIGHT - 26;   /* SHADOWS row */

        /* AMB label click → select ambient adjustment */
        if (fx >= bx && fx <= bx + bw && fy >= by0 && fy <= by0 + bh) {
            adjusting_ambient = true;
            return;
        }
        /* AMB [-] */
        if (fx >= bx + bw + 2 && fx <= bx + bw + 14 && fy >= by0 && fy <= by0 + bh) {
            pal_ambient -= 0.02f;
            if (pal_ambient < 0.0f) pal_ambient = 0.0f;
            return;
        }
        /* AMB [+] */
        if (fx >= bx + bw + 16 && fx <= bx + bw + 28 && fy >= by0 && fy <= by0 + bh) {
            pal_ambient += 0.02f;
            if (pal_ambient > 1.0f) pal_ambient = 1.0f;
            return;
        }
        /* LIGHT label click → select light adjustment */
        if (fx >= bx && fx <= bx + bw && fy >= by1 && fy <= by1 + bh) {
            adjusting_ambient = false;
            return;
        }
        /* LIGHT [-] */
        if (fx >= bx + bw + 2 && fx <= bx + bw + 14 && fy >= by1 && fy <= by1 + bh) {
            pal_light_mult -= 0.1f;
            if (pal_light_mult < 0.0f) pal_light_mult = 0.0f;
            return;
        }
        /* LIGHT [+] */
        if (fx >= bx + bw + 16 && fx <= bx + bw + 28 && fy >= by1 && fy <= by1 + bh) {
            pal_light_mult += 0.1f;
            if (pal_light_mult > 5.0f) pal_light_mult = 5.0f;
            return;
        }
        /* SHADING toggle */
        if (fx >= bx && fx <= bx + 90 && fy >= by2 && fy <= by2 + bh) {
            pixel_lighting = !pixel_lighting;
            return;
        }
        /* SHADOWS toggle */
        if (fx >= bx && fx <= bx + 78 && fy >= by3 && fy <= by3 + bh) {
            shadows_enabled = !shadows_enabled;
            return;
        }
        } /* end palette scene buttons */
    }
}

static void event(const sapp_event *ev) {
    /* ── Touch / mouse tap ─────────────────────────────────── */
    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        handle_tap(ev->mouse_x, ev->mouse_y);
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_BEGAN && ev->num_touches > 0) {
        handle_tap(ev->touches[0].pos_x, ev->touches[0].pos_y);
        return;
    }

    if (ev->type != SAPP_EVENTTYPE_KEY_DOWN) return;

    /* ── Global keys (work in any state) ────────────────────── */
    if (ev->key_code == SAPP_KEYCODE_8 && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
        if (!sr_gif_is_recording()) {
            gif_capture_timer = 0.0;
            sr_gif_start_recording(FB_WIDTH, FB_HEIGHT);
        }
        return;
    }
    if (ev->key_code == SAPP_KEYCODE_9 && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
        if (sr_gif_is_recording()) sr_gif_stop_and_save();
        return;
    }
    if (ev->key_code == SAPP_KEYCODE_P && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
        save_screenshot(fb.color, FB_WIDTH, FB_HEIGHT);
        return;
    }

    /* ── Menu state ─────────────────────────────────────────── */
    if (app_state == STATE_MENU) {
        switch (ev->key_code) {
            case SAPP_KEYCODE_UP:
                if (menu_cursor > 0) menu_cursor--;
                break;
            case SAPP_KEYCODE_DOWN:
                if (menu_cursor < SCENE_COUNT - 1) menu_cursor++;
                break;
            case SAPP_KEYCODE_ENTER:
            case SAPP_KEYCODE_KP_ENTER:
                current_scene = menu_cursor;
                app_state = STATE_RUNNING;
                break;
            case SAPP_KEYCODE_1:
                current_scene = SCENE_NEIGHBORHOOD;
                menu_cursor = SCENE_NEIGHBORHOOD;
                app_state = STATE_RUNNING;
                break;
            case SAPP_KEYCODE_2:
                current_scene = SCENE_CUBES;
                menu_cursor = SCENE_CUBES;
                app_state = STATE_RUNNING;
                break;
            case SAPP_KEYCODE_3:
                current_scene = SCENE_PALETTE_HOUSE;
                menu_cursor = SCENE_PALETTE_HOUSE;
                app_state = STATE_RUNNING;
                break;
            case SAPP_KEYCODE_4:
                current_scene = SCENE_DUNGEON;
                menu_cursor = SCENE_DUNGEON;
                app_state = STATE_RUNNING;
                break;
            case SAPP_KEYCODE_ESCAPE:
                app_state = STATE_RUNNING;
                break;
            default: break;
        }
        return;
    }

    /* ── Running state ──────────────────────────────────────── */
    switch (ev->key_code) {
        case SAPP_KEYCODE_TAB:
            app_state = STATE_MENU;
            break;
        case SAPP_KEYCODE_ESCAPE:
            app_state = STATE_MENU;
            break;
        case SAPP_KEYCODE_N:
            if (current_scene == SCENE_NEIGHBORHOOD) {
                night_mode = !night_mode;
                uint32_t fog_col = night_mode ? NIGHT_SKY_COLOR : FOG_COLOR;
                sr_fog_set(fog_col, FOG_NEAR, FOG_FAR);
                if (!night_mode) num_lights = 0;
            }
            break;
        case SAPP_KEYCODE_L:
            if (current_scene == SCENE_PALETTE_HOUSE) {
                adjusting_ambient = !adjusting_ambient;
            }
            break;
        case SAPP_KEYCODE_V:
            if (current_scene == SCENE_PALETTE_HOUSE) {
                pixel_lighting = !pixel_lighting;
            }
            break;
        case SAPP_KEYCODE_F:
            if (current_scene == SCENE_DUNGEON) {
                dng_light_mode = (dng_light_mode + 1) % 2;
            }
            break;
        case SAPP_KEYCODE_S:
            if (current_scene == SCENE_PALETTE_HOUSE) {
                shadows_enabled = !shadows_enabled;
            } else if (current_scene == SCENE_DUNGEON && dng_play_state == DNG_STATE_PLAYING) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    (dng_state.player.dir + 2) % 4);
            }
            break;
        case SAPP_KEYCODE_EQUAL:        /* + key (=/+) */
        case SAPP_KEYCODE_KP_ADD:
            if (current_scene == SCENE_PALETTE_HOUSE) {
                if (adjusting_ambient) {
                    pal_ambient += 0.02f;
                    if (pal_ambient > 1.0f) pal_ambient = 1.0f;
                } else {
                    pal_light_mult += 0.1f;
                    if (pal_light_mult > 5.0f) pal_light_mult = 5.0f;
                }
            } else if (current_scene == SCENE_DUNGEON) {
                dng_cfg.light_brightness += 0.1f;
                if (dng_cfg.light_brightness > 5.0f) dng_cfg.light_brightness = 5.0f;
            }
            break;
        case SAPP_KEYCODE_MINUS:
        case SAPP_KEYCODE_KP_SUBTRACT:
            if (current_scene == SCENE_PALETTE_HOUSE) {
                if (adjusting_ambient) {
                    pal_ambient -= 0.02f;
                    if (pal_ambient < 0.0f) pal_ambient = 0.0f;
                } else {
                    pal_light_mult -= 0.1f;
                    if (pal_light_mult < 0.0f) pal_light_mult = 0.0f;
                }
            } else if (current_scene == SCENE_DUNGEON) {
                dng_cfg.light_brightness -= 0.1f;
                if (dng_cfg.light_brightness < 0.0f) dng_cfg.light_brightness = 0.0f;
            }
            break;
        /* ── Dungeon movement ──────────────────────────────────── */
        case SAPP_KEYCODE_W:
        case SAPP_KEYCODE_UP:
            if (current_scene == SCENE_DUNGEON && dng_play_state == DNG_STATE_PLAYING) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    dng_state.player.dir);
            }
            break;
        case SAPP_KEYCODE_DOWN:
            if (current_scene == SCENE_DUNGEON && dng_play_state == DNG_STATE_PLAYING) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    (dng_state.player.dir + 2) % 4);
            } else if (app_state == STATE_MENU) {
                /* already handled above */
            }
            break;
        case SAPP_KEYCODE_A:
            if (current_scene == SCENE_DUNGEON && dng_play_state == DNG_STATE_PLAYING) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    (dng_state.player.dir + 3) % 4);
            }
            break;
        case SAPP_KEYCODE_D:
            if (current_scene == SCENE_DUNGEON && dng_play_state == DNG_STATE_PLAYING) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    (dng_state.player.dir + 1) % 4);
            }
            break;
        case SAPP_KEYCODE_LEFT:
            if (current_scene == SCENE_DUNGEON && dng_play_state == DNG_STATE_PLAYING) {
                dng_state.player.dir = (dng_state.player.dir + 3) % 4;
                dng_state.player.target_angle -= 0.25f;
            }
            break;
        case SAPP_KEYCODE_RIGHT:
            if (current_scene == SCENE_DUNGEON && dng_play_state == DNG_STATE_PLAYING) {
                dng_state.player.dir = (dng_state.player.dir + 1) % 4;
                dng_state.player.target_angle += 0.25f;
            }
            break;
        case SAPP_KEYCODE_Q:
            if (current_scene == SCENE_DUNGEON && dng_play_state == DNG_STATE_PLAYING) {
                dng_state.player.dir = (dng_state.player.dir + 3) % 4;
                dng_state.player.target_angle -= 0.25f;
            }
            break;
        case SAPP_KEYCODE_E:
            if (current_scene == SCENE_DUNGEON && dng_play_state == DNG_STATE_PLAYING) {
                dng_state.player.dir = (dng_state.player.dir + 1) % 4;
                dng_state.player.target_angle += 0.25f;
            }
            break;
        default: break;
    }
}

sapp_desc sokol_main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb    = init,
        .frame_cb   = frame,
        .cleanup_cb = cleanup,
        .event_cb   = event,
        .width      = FB_WIDTH * 2,
        .height     = FB_HEIGHT * 2,
        .window_title = "StarRaster",
        .logger.func  = slog_func,
        .swap_interval = 0,  /* software frame limiter handles 60fps cap */
    };
}
