
#include "cmath.h"
#include "platform.h"
#include "renderer.h"

#include <glad/glad.h>

#include <ft2build.h>
#include FT_FREETYPE_H

// TODO(etakarinaee): i want to get rid of this dependency
#include <stbi/stb_image.h>

// TODO(ttchef) move this out so its clean xD
void get_window_size(u32 *w, u32 *h);

// resources

typedef struct {
    GLuint program;
    GLuint vao;
    GLuint vbo;
} pipeline;

typedef struct { // number of floats
    u32 size;
} attribute;

static pipeline create_pipeline(const char *vertex_source,
                                const char *fragment_source,
                                const attribute *attributes, const u32 vertices,
                                const GLenum usage) {
    pipeline pipeline;

    const GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vertex_source, NULL);
    glCompileShader(vertex);

    const GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fragment_source, NULL);
    glCompileShader(fragment);

    pipeline.program = glCreateProgram();
    glAttachShader(pipeline.program, vertex);
    glAttachShader(pipeline.program, fragment);
    glLinkProgram(pipeline.program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    const int n = 1;
    glGenVertexArrays(n, &pipeline.vao);
    glGenBuffers(n, &pipeline.vbo);

    glBindVertexArray(pipeline.vao);
    glBindBuffer(GL_ARRAY_BUFFER, pipeline.vbo);

    u32 stride = 0;
    for (const attribute *a = attributes; a->size != 0; a++) {
        stride += a->size;
    }

    const GLsizeiptr buffer_size =
        (GLsizeiptr)(sizeof(float) * stride * vertices);
    glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, usage);

    u32 offset = 0;
    for (u32 i = 0; attributes[i].size != 0; i++) {
        const int normalized = GL_FALSE;
        glVertexAttribPointer(i, (GLint)attributes[i].size, GL_FLOAT,
                              normalized, (GLsizei)(stride * sizeof(float)),
                              (void *)(offset * sizeof(float)));

        glEnableVertexAttribArray(i);
        offset += attributes[i].size;
    }

    return pipeline;
}

static u0 destroy_pipeline(const pipeline *pipeline) {
    const int n = 1;
    glDeleteBuffers(n, &pipeline->vbo);
    glDeleteVertexArrays(n, &pipeline->vao);
    glDeleteProgram(pipeline->program);
}

static pipeline mesh;
static pipeline text;

static FT_Library ft;

// shaders

static const char *text_vertex_shader_source =
    "#version 330 core\n"
    "layout (location = 0) in vec2 position;\n"
    "layout (location = 1) in vec2 uv;\n"
    "out vec2 tex_coord;\n"
    "uniform vec2 window_size;\n"
    "void main() {\n"
    "    vec2 ndc = (position / window_size) * 2.0 - 1.0;\n"
    "    ndc.y = -ndc.y;\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "    tex_coord = uv;\n"
    "}\n";

static const char *text_fragment_shader_source =
    "#version 330 core\n"
    "in vec2 tex_coord;\n"
    "uniform sampler2D glyph_texture;\n"
    "uniform vec4 color;\n"
    "uniform float smoothing;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    float distance = texture(glyph_texture, tex_coord).r;\n"
    "    float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, "
    "distance);\n"
    "    if (alpha < 0.01) discard;\n"
    "    frag_color = vec4(color.rgb, color.a * alpha);\n"
    "}\n";

static const char *mesh_vertex_shader =
    "#version 330 core\n"
    "layout (location = 0) in vec3 position;\n"
    "layout (location = 1) in vec2 uv;\n"
    "out vec2 tex_coord;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "    gl_Position = mvp * vec4(position, 1.0);\n"
    "    tex_coord = uv;\n"
    "}\n";

static const char *mesh_fragment_shader =
    "#version 330 core\n"
    "in vec2 tex_coord;\n"
    "uniform sampler2D mesh_texture;\n"
    "uniform vec4 color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(mesh_texture, tex_coord) * color;\n"
    "}\n";

// pos(3) uv(2)
static const f32 quad_vertices[] = {
    -1, -1, 0, 0, 0, 1, -1, 0, 1, 0, 1,  1, 0, 1, 1,
    -1, -1, 0, 0, 0, 1, 1,  0, 1, 1, -1, 1, 0, 0, 1,
};

//

typedef struct {
    GLuint texture;
    u32 width;
    u32 height;
    i32 bearing_x;
    i32 bearing_y;
    u32 advance;
} glyph;

typedef struct {
    glyph glyphs[128];
    u32 size;
    u32 sdf_size;
} font;

#define FONTS_CAPACITY 16
static font fonts[FONTS_CAPACITY];
static u32 font_count = 0;

#define TEXTURES_CAPACITY 64
static GLuint textures[TEXTURES_CAPACITY];
static u32 texture_count = 0;

static GLuint magic_pixel = 0;

// SDF generation size - render glyphs at this size for SDF, then scale at draw
// time
#define SDF_RENDER_SIZE 64
#define SDF_SPREAD 8

u0 initialize_gl(u0) {
    if (!gladLoadGLLoader((GLADloadproc)get_proc_address())) {
        fprintf(stderr, "failed to initialize glad\n");
        return;
    }

    text =
        create_pipeline(text_vertex_shader_source, text_fragment_shader_source,
                        (attribute[]){{2}, {2}, {0}}, // pos(2), uv(2)
                        6, GL_DYNAMIC_DRAW);

    mesh = create_pipeline(mesh_vertex_shader, mesh_fragment_shader,
                           (attribute[]){{3}, {2}, {0}}, // pos(3), uv(2)
                           4096, GL_DYNAMIC_DRAW);

    FT_Init_FreeType(&ft);

    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const int n = 1;
    glGenTextures(n, &magic_pixel);
    glBindTexture(GL_TEXTURE_2D, magic_pixel);
    const u8 white_pixel[] = {255, 255, 255, 255};
    const int level = 0;
    const int border = 0;
    glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, 1, 1, border, GL_RGBA,
                 GL_UNSIGNED_BYTE, white_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

u0 quit_gl(u0) {
    for (u32 i = 0; i < font_count; i++) {
        for (u32 c = 0; c < 128; c++) {
            if (fonts[i].glyphs[c].texture) {
                const int n = 1;
                glDeleteTextures(n, &fonts[i].glyphs[c].texture);
            }
        }
    }

    for (u32 i = 0; i < texture_count; i++) {
        if (textures[i]) {
            const int n = 1;
            glDeleteTextures(n, &textures[i]);
        }
    }

    if (magic_pixel) {
        const int n = 1;
        glDeleteTextures(n, &magic_pixel);
    }

    FT_Done_FreeType(ft);

    destroy_pipeline(&text);
    destroy_pipeline(&mesh);
}

//

font_id font_initialize(const char *path, const u32 size) {
    if (font_count >= FONTS_CAPACITY) {
        return 0;
    }

    FT_Face face;
    const int face_index = 0;
    if (FT_New_Face(ft, path, face_index, &face)) {
        return 0;
    }

    const int pixel_width = 0;
    FT_Set_Pixel_Sizes(face, pixel_width, SDF_RENDER_SIZE);

    const font_id id = font_count;
    font *font = &fonts[font_count++];
    font->size = size;
    font->sdf_size = SDF_RENDER_SIZE;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (u32 c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_DEFAULT)) {
            font->glyphs[c].texture = 0;
            continue;
        }

        // TODO(etakarinaee): this takes long, optimize somehow so there isn't a
        // second of nothingness
        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF)) {
            font->glyphs[c].texture = 0;
            continue;
        }

        GLuint texture;

        const int n = 1;
        glGenTextures(n, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        const int level = 0;
        const int border = 0;
        glTexImage2D(GL_TEXTURE_2D, level, GL_RED,
                     (GLsizei)face->glyph->bitmap.width,
                     (GLsizei)face->glyph->bitmap.rows, border, GL_RED,
                     GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        font->glyphs[c].texture = texture;
        font->glyphs[c].width = face->glyph->bitmap.width;
        font->glyphs[c].height = face->glyph->bitmap.rows;
        font->glyphs[c].bearing_x = face->glyph->bitmap_left;
        font->glyphs[c].bearing_y = face->glyph->bitmap_top;
        font->glyphs[c].advance = face->glyph->advance.x >> 6;
    }

    FT_Done_Face(face);

    return id;
}

u0 font_deinitialize(const font_id font) {
    if (font >= font_count) {
        return;
    }

    for (u32 c = 0; c < 128; c++) {
        if (fonts[font].glyphs[c].texture) {
            const int n = 1;

            glDeleteTextures(n, &fonts[font].glyphs[c].texture);
            fonts[font].glyphs[c].texture = 0;
        }
    }
}

//

texture_id texture_initialize(const char *path) {
    if (texture_count >= TEXTURES_CAPACITY) {
        return 0;
    }

    int width, height, channels;
    stbi_set_flip_vertically_on_load(1);

    const int desired_channels = 0;
    unsigned char *data =
        stbi_load(path, &width, &height, &channels, desired_channels);

    if (!data) {
        return 0;
    }

    GLenum format;
    if (channels == 1) {
        format = GL_RED;
    } else if (channels == 3) {
        format = GL_RGB;
    } else {
        format = GL_RGBA;
    }

    GLuint gl_texture;
    const int n = 1;
    glGenTextures(n, &gl_texture);
    glBindTexture(GL_TEXTURE_2D, gl_texture);

    const int level = 0;
    const int border = 0;
    glTexImage2D(GL_TEXTURE_2D, level, (GLint)format, width, height, border,
                 format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    const texture_id id = texture_count;
    textures[texture_count++] = gl_texture;
    return id;
}

u0 texture_deinitialize(const texture_id texture) {
    if (texture >= texture_count) {
        return;
    }

    if (textures[texture]) {
        const int n = 1;
        glDeleteTextures(n, &textures[texture]);
        textures[texture] = 0;
    }
}

u0 clear_color(const color color) {
    const float red = (f32)(color >> 24 & 0xFF) / 255.0f;
    const f32 green = (f32)(color >> 16 & 0xFF) / 255.0f;
    const f32 blue = (f32)(color >> 8 & 0xFF) / 255.0f;
    const f32 alpha = (f32)(color & 0xFF) / 255.0f;

    glClearColor(red, green, blue, alpha);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

u0 draw_text(const char *string, u32 x, const u32 y, const font_id id,
             const f32 scale, const color color) {
    if (id >= font_count) {
        return;
    }

    const f32 r = (f32)(color >> 24 & 0xFF) / 255.0f;
    const f32 g = (f32)(color >> 16 & 0xFF) / 255.0f;
    const f32 b = (f32)(color >> 8 & 0xFF) / 255.0f;
    const f32 a = (f32)(color & 0xFF) / 255.0f;

    u32 w, h;
    get_window_size(&w, &h);

    const font *font = &fonts[id];

    const f32 sdf_scale = ((f32)font->size / (f32)font->sdf_size) * scale;

    const f32 pixel_size = (f32)font->sdf_size * sdf_scale;
    const f32 smoothing = 3.0f / pixel_size;

    glUseProgram(text.program);
    glUniform2f(glGetUniformLocation(text.program, "window_size"), (f32)w,
                (f32)h);
    glUniform4f(glGetUniformLocation(text.program, "color"), r, g, b, a);
    glUniform1f(glGetUniformLocation(text.program, "smoothing"), smoothing);

    glBindVertexArray(text.vao);
    glBindBuffer(GL_ARRAY_BUFFER, text.vbo);

    for (const char *c = string; *c; c++) {
        const u8 ch = (u8)*c;
        if (ch >= 128) {
            continue;
        }

        const glyph *glyph = &font->glyphs[ch];
        if (!glyph->texture) {
            x += (u32)((f32)(font->size / 2.0) * scale);

            continue;
        }

        const f32 xpos = (f32)(x + (i32)((f32)glyph->bearing_x * sdf_scale));
        const f32 ypos = (f32)(y - (i32)((f32)glyph->bearing_y * sdf_scale) +
                               (i32)((f32)font->size * scale));

        const f32 gw = (f32)glyph->width * sdf_scale;
        const f32 gh = (f32)glyph->height * sdf_scale;

        const f32 vertices[] = {
            xpos,      ypos,      0.0f, 0.0f, xpos + gw, ypos,      1.0f, 0.0f,
            xpos + gw, ypos + gh, 1.0f, 1.0f, xpos,      ypos,      0.0f, 0.0f,
            xpos + gw, ypos + gh, 1.0f, 1.0f, xpos,      ypos + gh, 0.0f, 1.0f,
        };

        glBindTexture(GL_TEXTURE_2D, glyph->texture);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (u32)((f32)glyph->advance * sdf_scale);
    }
}

u0 draw_mesh(const f32 *vertices, const u32 vertex_count, const f32 *mvp,
             const texture_id texture, const color color) {
    GLuint gl_texture;

    if (texture == NO_TEXTURE) {
        gl_texture = magic_pixel;
    } else if (texture >= texture_count || !textures[texture]) {
        return;
    } else {
        gl_texture = textures[texture];
    }

    const f32 r = (f32)(color >> 24 & 0xFF) / 255.0f;
    const f32 g = (f32)(color >> 16 & 0xFF) / 255.0f;
    const f32 b = (f32)(color >> 8 & 0xFF) / 255.0f;
    const f32 a = (f32)(color & 0xFF) / 255.0f;

    glUseProgram(mesh.program);
    const int transpose = GL_FALSE;
    const int count = 1;
    glUniformMatrix4fv(glGetUniformLocation(mesh.program, "mvp"), count,
                       transpose, mvp);
    glUniform4f(glGetUniformLocation(mesh.program, "color"), r, g, b, a);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glUniform1i(glGetUniformLocation(mesh.program, "mesh_texture"), 0);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);

    const int offset = 0;
    glBufferSubData(GL_ARRAY_BUFFER, offset,
                    (GLsizeiptr)(sizeof(f32) * vertex_count * 5), vertices);

    const int first = 0;
    glDrawArrays(GL_TRIANGLES, first, (GLsizei)vertex_count);
}

u0 draw_render_obj(render_obj *obj) {
    u32 w, h;
    get_window_size(&w, &h);

    switch (obj->type) {
    case RENDER_OBJ_TYPE_QUAD: {
        matrix scale;
        math_matrix_scale(&scale, obj->scale, obj->scale, 1.0);

        matrix trans;
        math_matrix_translate(&trans, obj->pos.x, obj->pos.y, 0.0f);

        matrix model;
        math_matrix_mul(&model, &trans, &scale);

        matrix view;
        math_matrix_get_orthographic(w, h, &view);

        matrix m;
        math_matrix_mul(&m, &view, &model);

        draw_mesh(quad_vertices, 6, m.m, NO_TEXTURE, obj->quad.color);
    } break;
    case RENDER_OBJ_TYPE_TEXTURE: {
        matrix scale;
        math_matrix_scale(&scale, obj->scale, obj->scale, 1.0);

        matrix trans;
        math_matrix_translate(&trans, obj->pos.x, obj->pos.y, 0.0f);

        matrix model;
        math_matrix_mul(&model, &trans, &scale);

        matrix view;
        math_matrix_get_orthographic(w, h, &view);

        matrix m;
        math_matrix_mul(&m, &view, &model);

        draw_mesh(quad_vertices, 6, m.m, obj->texture.id, WHITE);
    } break;
    case RENDER_OBJ_TYPE_TEXT: {
        draw_text(obj->text.chars, obj->pos.x, obj->pos.y, obj->text.font,
                  obj->scale, obj->text.color);
    } break;
    }
}

u0 measure_text(const char *string, const font_id id, const f32 scale,
                u32 *width, u32 *height) {
    *width = 0;
    *height = 0;

    if (id >= font_count) {
        return;
    }

    const font *f = &fonts[id];
    const f32 sdf_scale = (f32)f->size / (f32)f->sdf_size * scale;

    for (const char *c = string; *c; c++) {
        const u8 ch = (u8)*c;
        if (ch >= 128) {
            continue;
        }

        const glyph *g = &f->glyphs[ch];
        if (!g->texture) {
            *width += (u32)((f32)(f->size / 2.0) * scale);

            continue;
        }

        *width += (u32)((f32)g->advance * sdf_scale);

        const u32 glyph_height = (u32)((f32)g->height * sdf_scale);

        if (glyph_height > *height) {
            *height = glyph_height;
        }
    }

    if (*height == 0) {
        *height = (u32)((f32)f->size * scale);
    }
}
