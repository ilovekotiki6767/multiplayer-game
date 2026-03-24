

#ifndef CMAHTH_H
#define CMAHTH_H

#include "types.h"

#define RAD2DEG(x) (x * 180.0f / M_PI)
#define DEG2RAD(x) (x * M_PI / 180.0f)

typedef struct {
    float x;
    float y;
} vec2;

typedef struct {
    int x;
    int y;
} vec2i;

typedef struct {
    float x;
    float y;
    float z;
    float w;
} vec4;

#define VEC2(x, y)                                                             \
    (vec2) { (x), (y) }

#define VEC2I(x, y)                                                            \
    (vec2i) { (x), (y) }

#define VEC4(x, y, z, w)                                                       \
    (vec4) { (x), (y), (z), (w) }

typedef struct {
    float m[16];
} matrix;

float math_clamp(float n, float lower, float upper);

vec2 math_vec2_add(vec2 a, vec2 b);
vec2 math_vec2_subtract(vec2 a, vec2 b);
vec2 math_vec2_scale(vec2 v, float scalar);
vec2 math_vec2_rotate(vec2 v, float angle);

float math_vec2_length(vec2 v);
vec2 math_vec2_norm(vec2 v);
float math_vec2_distance(vec2 a, vec2 b);
float math_vec2_dot(vec2 a, vec2 b);
float math_vec2_angle_cos(vec2 a, vec2 b);

/* Angle in degrees */
float math_vec2_angle(vec2 a, vec2 b);
vec2 math_vec2_mul_matrix(vec2 v, matrix *m);

vec2i math_vec2_to_vec2i(vec2 v);
vec2 math_vec2i_to_vec2(vec2i v);

void math_matrix_identity(matrix *m);
void math_matrix_translate(matrix *m, float x, float y, float z);
void math_matrix_scale(matrix *m, float x, float y, float z);

/* Angle in degrees */
void math_matrix_rotate_2d(matrix *m, float angle);
void math_matrix_mul(matrix *out, matrix *a, matrix *b);

/* utility */
void math_vec2_print(vec2 v);
void math_vec4_print(vec4 v);
void math_matrix_print(matrix *m);

void math_matrix_orthographic(matrix *m, float left, float right, float bottom,
                              float top, float near, float far);
void math_matrix_get_orthographic(u32 w, u32 h, matrix *m);

#endif // CMAHTH_H
