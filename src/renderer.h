#ifndef RENDERER_H
#define RENDERER_H

#include "cmath.h"
#include "types.h"

#include <string.h>

typedef u32 color;
#define BLACK 0x000000FF
#define RED 0xFF0000FF
#define WHITE 0xFFFFFFFF

typedef u32 font_id;
typedef u32 texture_id;
#define NO_TEXTURE UINT32_MAX

#define MAX_CHARS 128

enum {
    RENDER_OBJ_TYPE_QUAD,
    RENDER_OBJ_TYPE_TEXTURE,
    RENDER_OBJ_TYPE_TEXT,
};

typedef struct {
    int type;
    vec2 pos;
    f32 scale;

    union {
        struct {
            color color;
        } quad;
        struct {
            texture_id id;
        } texture;

        struct {
            // needs to be array because we need to send the data over network
            char chars[MAX_CHARS];
            font_id font;
            color color;
        } text;
    };
} render_obj;

static inline render_obj QUAD(vec2 pos, f32 scale, color color) {
    return (render_obj){.type = RENDER_OBJ_TYPE_QUAD,
                        .pos = pos,
                        .scale = scale,
                        .quad.color = color};
}

static inline render_obj TEXTURE(vec2 pos, f32 scale, texture_id id) {
    return (render_obj){.type = RENDER_OBJ_TYPE_TEXTURE,
                        .pos = pos,
                        .scale = scale,
                        .texture.id = id};
}

static inline render_obj TEXT(vec2 pos, f32 scale, const char *text,
                              font_id font, color color) {
    render_obj obj = (render_obj){.type = RENDER_OBJ_TYPE_TEXT,
                                  .pos = pos,
                                  .scale = scale,
                                  .text.font = font,
                                  .text.color = color};
    u32 min_size =
        strlen(text) < (MAX_CHARS - 1) ? strlen(text) : MAX_CHARS - 1;
    strncpy(obj.text.chars, text, min_size);
    obj.text.chars[min_size] = '\0';
    return obj;
}

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

u0 draw_render_obj(render_obj *obj);

u0 initialize_gl(u0);

u0 clear_color(color color);

u0 quit_gl(u0);

#endif // RENDERER
