#ifndef RENDERER_H
#define RENDERER_H

#include "cmath.h"
#include "types.h"

typedef u32 color;
#define BLACK 0x000000FF
#define RED 0xFF0000FF
#define WHITE 0xFFFFFFFF

typedef u32 font_id;
typedef u32 texture_id;
#define NO_TEXTURE UINT32_MAX

font_id font_initialize(const char *path, u32 size);

u0 font_deinitialize(font_id font);

texture_id texture_initialize(const char *path);

u0 texture_deinitialize(texture_id texture);

u0 draw_text(const char *string, u32 x, u32 y, font_id id, f32 scale,
             color color);

u0 measure_text(const char *string, font_id id, f32 scale, u32 *width,
                u32 *height);

// if `texture` is `NO_TEXTURE`, uses `color` as a solid fill color
// otherwise, samples from `texture` and multiplies by `color`
u0 draw_mesh(const f32 *vertices, u32 vertex_count, const f32 *mvp,
             texture_id texture, color color);

u0 initialize_gl(u0);

u0 clear_color(color color);

u0 quit_gl(u0);

#endif // RENDERER
