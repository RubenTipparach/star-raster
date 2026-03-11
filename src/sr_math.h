#ifndef SR_MATH_H
#define SR_MATH_H

#include <math.h>
#include <string.h>

/* ── Vector types ────────────────────────────────────────────────── */

typedef struct { float x, y; } sr_vec2;
typedef struct { float x, y, z; } sr_vec3;
typedef struct { float x, y, z, w; } sr_vec4;

/* Column-major 4x4 matrix (m[col][row]) */
typedef struct { float m[4][4]; } sr_mat4;

/* ── Vec3 ops ────────────────────────────────────────────────────── */

static inline sr_vec3 sr_v3(float x, float y, float z) {
    return (sr_vec3){x, y, z};
}

static inline sr_vec3 sr_v3_add(sr_vec3 a, sr_vec3 b) {
    return (sr_vec3){a.x+b.x, a.y+b.y, a.z+b.z};
}

static inline sr_vec3 sr_v3_sub(sr_vec3 a, sr_vec3 b) {
    return (sr_vec3){a.x-b.x, a.y-b.y, a.z-b.z};
}

static inline sr_vec3 sr_v3_scale(sr_vec3 v, float s) {
    return (sr_vec3){v.x*s, v.y*s, v.z*s};
}

static inline float sr_v3_dot(sr_vec3 a, sr_vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static inline sr_vec3 sr_v3_cross(sr_vec3 a, sr_vec3 b) {
    return (sr_vec3){
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
}

static inline float sr_v3_length(sr_vec3 v) {
    return sqrtf(sr_v3_dot(v, v));
}

static inline sr_vec3 sr_v3_normalize(sr_vec3 v) {
    float len = sr_v3_length(v);
    if (len < 1e-8f) return (sr_vec3){0,0,0};
    return sr_v3_scale(v, 1.0f / len);
}

/* ── Vec4 ops ────────────────────────────────────────────────────── */

static inline sr_vec4 sr_v4(float x, float y, float z, float w) {
    return (sr_vec4){x, y, z, w};
}

/* ── Mat4 ops ────────────────────────────────────────────────────── */

static inline sr_mat4 sr_mat4_identity(void) {
    sr_mat4 r;
    memset(&r, 0, sizeof(r));
    r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
    return r;
}

static inline sr_mat4 sr_mat4_mul(sr_mat4 a, sr_mat4 b) {
    sr_mat4 r;
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            r.m[c][row] = a.m[0][row]*b.m[c][0]
                        + a.m[1][row]*b.m[c][1]
                        + a.m[2][row]*b.m[c][2]
                        + a.m[3][row]*b.m[c][3];
        }
    }
    return r;
}

static inline sr_vec4 sr_mat4_mul_v4(sr_mat4 m, sr_vec4 v) {
    return (sr_vec4){
        m.m[0][0]*v.x + m.m[1][0]*v.y + m.m[2][0]*v.z + m.m[3][0]*v.w,
        m.m[0][1]*v.x + m.m[1][1]*v.y + m.m[2][1]*v.z + m.m[3][1]*v.w,
        m.m[0][2]*v.x + m.m[1][2]*v.y + m.m[2][2]*v.z + m.m[3][2]*v.w,
        m.m[0][3]*v.x + m.m[1][3]*v.y + m.m[2][3]*v.z + m.m[3][3]*v.w
    };
}

static inline sr_mat4 sr_mat4_translate(float tx, float ty, float tz) {
    sr_mat4 r = sr_mat4_identity();
    r.m[3][0] = tx;
    r.m[3][1] = ty;
    r.m[3][2] = tz;
    return r;
}

static inline sr_mat4 sr_mat4_scale(float sx, float sy, float sz) {
    sr_mat4 r = sr_mat4_identity();
    r.m[0][0] = sx;
    r.m[1][1] = sy;
    r.m[2][2] = sz;
    return r;
}

static inline sr_mat4 sr_mat4_rotate_y(float radians) {
    sr_mat4 r = sr_mat4_identity();
    float c = cosf(radians), s = sinf(radians);
    r.m[0][0] =  c;  r.m[2][0] = s;
    r.m[0][2] = -s;  r.m[2][2] = c;
    return r;
}

static inline sr_mat4 sr_mat4_rotate_x(float radians) {
    sr_mat4 r = sr_mat4_identity();
    float c = cosf(radians), s = sinf(radians);
    r.m[1][1] =  c;  r.m[2][1] = -s;
    r.m[1][2] =  s;  r.m[2][2] =  c;
    return r;
}

/* Perspective projection (OpenGL NDC: z in [-1, 1]) */
static inline sr_mat4 sr_mat4_perspective(float fov_y_rad, float aspect, float znear, float zfar) {
    sr_mat4 r;
    memset(&r, 0, sizeof(r));
    float f = 1.0f / tanf(fov_y_rad * 0.5f);
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = (zfar + znear) / (znear - zfar);
    r.m[2][3] = -1.0f;
    r.m[3][2] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

/* Look-at view matrix */
static inline sr_mat4 sr_mat4_lookat(sr_vec3 eye, sr_vec3 target, sr_vec3 up) {
    sr_vec3 f = sr_v3_normalize(sr_v3_sub(target, eye));
    sr_vec3 s = sr_v3_normalize(sr_v3_cross(f, up));
    sr_vec3 u = sr_v3_cross(s, f);

    sr_mat4 r = sr_mat4_identity();
    r.m[0][0] =  s.x;  r.m[1][0] =  s.y;  r.m[2][0] =  s.z;
    r.m[0][1] =  u.x;  r.m[1][1] =  u.y;  r.m[2][1] =  u.z;
    r.m[0][2] = -f.x;  r.m[1][2] = -f.y;  r.m[2][2] = -f.z;
    r.m[3][0] = -sr_v3_dot(s, eye);
    r.m[3][1] = -sr_v3_dot(u, eye);
    r.m[3][2] =  sr_v3_dot(f, eye);
    return r;
}

#endif /* SR_MATH_H */
