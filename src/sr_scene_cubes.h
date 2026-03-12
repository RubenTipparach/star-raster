/*  sr_scene_cubes.h — 5000 Cubes scene.
 *  Single-TU header-only. Depends on sr_scene_neighborhood.h (draw_cube). */
#ifndef SR_SCENE_CUBES_H
#define SR_SCENE_CUBES_H

#define NUM_CUBES 5000

typedef struct {
    float x, y, z;
    float rot_y, rot_speed;
    float scale;
    int   tex_id;
} cube_instance;

static cube_instance cube_data[NUM_CUBES];
static bool cubes_initialized = false;

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

#endif /* SR_SCENE_CUBES_H */
