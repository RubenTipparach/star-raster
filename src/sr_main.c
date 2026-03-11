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

enum { SCENE_NEIGHBORHOOD, SCENE_CUBES, SCENE_PALETTE_HOUSE, SCENE_COUNT };
enum { STATE_MENU, STATE_RUNNING };

static int  app_state    = STATE_MENU;
static int  current_scene = SCENE_NEIGHBORHOOD;
static int  menu_cursor   = 0;

static const char *scene_names[] = {
    "NEIGHBORHOOD",
    "5000 CUBES",
    "PALETTE HOUSE",
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
#define PAL_AMBIENT        0.08f
#define PAL_DIFFUSE        0.0f        /* no sun in palette night scene */
#define NIGHT_AMBIENT     0.05f

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
    float total = PAL_AMBIENT;

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
            total += lum * atten * ldot;
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
    return (sr_vertex){ {lx, ly, lz}, {u, v}, col };
}

static void draw_palette_house(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                                float hx, float hz, float rot_y) {
    float W = 2.0f, D = 1.5f, WH = 2.0f, RP = 3.0f;

    #define PV(lx,ly,lz,u,v,nx,ny,nz) pal_house_vert(lx,ly,lz,u,v,nx,ny,nz,hx,hz,rot_y)

    float slope = (RP - WH) / W;
    float rn_len = sqrtf(1.0f + slope * slope);
    float rnx = 1.0f / rn_len, rny = slope / rn_len;

    /* Back wall */
    sr_draw_quad_indexed(fb_ptr,
        PV(-W, 0,  -D,  0,   WH/2,  0,0,-1),
        PV( W, 0,  -D,  W,   WH/2,  0,0,-1),
        PV( W, WH, -D,  W,   0,     0,0,-1),
        PV(-W, WH, -D,  0,   0,     0,0,-1),
        &itextures[ITEX_BRICK], mvp);

    /* Left wall */
    sr_draw_quad_indexed(fb_ptr,
        PV(-W, 0,   D,  0,    WH/2, -1,0,0),
        PV(-W, 0,  -D,  D,    WH/2, -1,0,0),
        PV(-W, WH, -D,  D,    0,    -1,0,0),
        PV(-W, WH,  D,  0,    0,    -1,0,0),
        &itextures[ITEX_BRICK], mvp);

    /* Right wall */
    sr_draw_quad_indexed(fb_ptr,
        PV( W, 0,  -D,  0,    WH/2,  1,0,0),
        PV( W, 0,   D,  D,    WH/2,  1,0,0),
        PV( W, WH,  D,  D,    0,     1,0,0),
        PV( W, WH, -D,  0,    0,     1,0,0),
        &itextures[ITEX_BRICK], mvp);

    /* Front wall (simplified, no door cutout for this demo) */
    sr_draw_quad_indexed(fb_ptr,
        PV( W, 0,   D,  0,   WH/2, 0,0,1),
        PV(-W, 0,   D,  W,   WH/2, 0,0,1),
        PV(-W, WH,  D,  W,   0,    0,0,1),
        PV( W, WH,  D,  0,   0,    0,0,1),
        &itextures[ITEX_BRICK], mvp);

    /* Door */
    {
        float dhw = 0.4f, dh = 1.6f;
        sr_draw_quad_indexed(fb_ptr,
            PV( dhw, 0,  D+0.01f, 0,1, 0,0,1),
            PV(-dhw, 0,  D+0.01f, 1,1, 0,0,1),
            PV(-dhw, dh, D+0.01f, 1,0, 0,0,1),
            PV( dhw, dh, D+0.01f, 0,0, 0,0,1),
            &itextures[ITEX_WOOD], mvp);
    }

    /* Gable ends */
    sr_draw_triangle_indexed(fb_ptr,
        PV( W, WH, D,  0,    1, 0,0,1),
        PV(-W, WH, D,  1,    1, 0,0,1),
        PV( 0, RP, D,  0.5f, 0, 0,0,1),
        &itextures[ITEX_BRICK], mvp);

    sr_draw_triangle_indexed(fb_ptr,
        PV(-W, WH, -D, 0,    1, 0,0,-1),
        PV( W, WH, -D, 1,    1, 0,0,-1),
        PV( 0, RP, -D, 0.5f, 0, 0,0,-1),
        &itextures[ITEX_BRICK], mvp);

    /* Roof slopes */
    {
        float oh = 0.3f;
        float ey = WH - oh * slope;

        sr_draw_quad_indexed_doublesided(fb_ptr,
            PV( 0,    RP, -D-oh,  0,    1, -rnx,rny,0),
            PV( 0,    RP,  D+oh,  1.5f, 1, -rnx,rny,0),
            PV(-W-oh, ey,  D+oh,  1.5f, 0, -rnx,rny,0),
            PV(-W-oh, ey, -D-oh,  0,    0, -rnx,rny,0),
            &itextures[ITEX_ROOF], mvp);

        sr_draw_quad_indexed_doublesided(fb_ptr,
            PV( 0,    RP,  D+oh,  0,    1,  rnx,rny,0),
            PV( 0,    RP, -D-oh,  1.5f, 1,  rnx,rny,0),
            PV( W+oh, ey, -D-oh,  1.5f, 0,  rnx,rny,0),
            PV( W+oh, ey,  D+oh,  0,    0,  rnx,rny,0),
            &itextures[ITEX_ROOF], mvp);
    }

    /* Windows */
    {
        float wy = 1.0f, wh = 0.6f, whw = 0.35f, wz = 0.0f;

        sr_draw_quad_indexed(fb_ptr,
            PV(-W-0.01f, wy,    wz+whw, 0,1, -1,0,0),
            PV(-W-0.01f, wy,    wz-whw, 1,1, -1,0,0),
            PV(-W-0.01f, wy+wh, wz-whw, 1,0, -1,0,0),
            PV(-W-0.01f, wy+wh, wz+whw, 0,0, -1,0,0),
            &itextures[ITEX_TILE], mvp);

        sr_draw_quad_indexed(fb_ptr,
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

static void draw_palette_scene(sr_framebuffer *fb_ptr, const sr_mat4 *vp, float t) {
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

                sr_draw_quad_indexed(fb_ptr,
                    sr_vert_c(x0, 0, z1, 0,ts, pal_intensity_color(i01)),
                    sr_vert_c(x0, 0, z0, 0, 0, pal_intensity_color(i00)),
                    sr_vert_c(x1, 0, z0, ts,0, pal_intensity_color(i10)),
                    sr_vert_c(x1, 0, z1, ts,ts, pal_intensity_color(i11)),
                    &itextures[ITEX_GRASS], vp);
            }
        }
    }

    /* Single house at origin */
    sr_mat4 mvp = *vp;  /* house at origin, no transform */
    draw_palette_house(fb_ptr, &mvp, 0.0f, 0.0f, 0.0f);

    /* Orbiting point light */
    num_lights = 1;
    float orbit_radius = 5.0f;
    float lx = cosf(t * 1.2f) * orbit_radius;
    float lz = sinf(t * 1.2f) * orbit_radius;
    float ly = 2.5f + sinf(t * 0.7f) * 1.0f;
    lights[0] = (point_light){ lx, ly, lz, 1.0f, 0.9f, 0.6f, 12.0f };
    build_light_grid();
}

/* ── Stats overlay ───────────────────────────────────────────────── */

static void draw_stats(sr_framebuffer *fb_ptr, int tris) {
    char buf[64];
    uint32_t white  = 0xFFFFFFFF;
    uint32_t shadow = 0xFF000000;

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

    /* Recording indicator */
    if (sr_gif_is_recording()) {
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            FB_WIDTH - 30, 3, "REC", 0xFF0000FF, shadow);
    }

    /* TAB hint */
    sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                        FB_WIDTH - 78, FB_HEIGHT - 12, "TAB = MENU", 0xFF999999, shadow);

    /* Night mode hint (neighborhood only) */
    if (current_scene == SCENE_NEIGHBORHOOD) {
        const char *ntxt = night_mode ? "N = DAY" : "N = NIGHT";
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            3, FB_HEIGHT - 12, ntxt, 0xFF999999, shadow);
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
    sr_draw_text_shadow(px, W, H, 130, 210, "TAB      MENU", gray, shadow);
    sr_draw_text_shadow(px, W, H, 130, 225, "ESC      QUIT", gray, shadow);
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

    /* Dithered distance fog — matches sky color */
    sr_fog_set(FOG_COLOR, FOG_NEAR, FOG_FAR);

    /* Pre-init cube scene data */
    cubes_scene_init();

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
    else if (night_mode && current_scene == SCENE_NEIGHBORHOOD)
        clear_color = NIGHT_SKY_COLOR;
    else
        clear_color = FOG_COLOR;
    sr_framebuffer_clear(&fb, clear_color, 1.0f);

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
        }
    }

    int tris = sr_stats_tri_count();
    draw_stats(&fb, tris);

    if (app_state == STATE_MENU)
        draw_menu(&fb);

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
    sg_update_image(fb_image, &(sg_image_data){
        .mip_levels[0] = { .ptr = fb.color, .size = FB_WIDTH * FB_HEIGHT * 4 }
    });

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
    sg_shutdown();
#ifdef _WIN32
    timeEndPeriod(1);
#endif
}

static void event(const sapp_event *ev) {
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
            case SAPP_KEYCODE_ESCAPE:
                sapp_request_quit();
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
