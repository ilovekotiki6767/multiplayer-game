#include <X11/X.h>
#include <X11/Xlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "cmath.h"
#include "common.h"
#include "net.h"
#include "platform.h"
#include "renderer.h"

#define BUF_SIZE 1024

// pos(3) uv(2)
static const f32 quad_vertices[] = {
    -1, -1, 0, 0, 0, 1, -1, 0, 1, 0, 1,  1, 0, 1, 1,
    -1, -1, 0, 0, 0, 1, 1,  0, 1, 1, -1, 1, 0, 0, 1,
};

i32 main(i32 argc, char **argv) {
    initialize();
    initialize_gl();

    char *ip = "127.0.0.1";
    if (argc > 1) {
        ip = argv[1];
    }

    net_client c;
    initialize_client(&c, ip, SERVER_PORT);

    char *message = "hello";
    size_t message_len = strlen(message);

    client_send_data(&c, NP_TYPE_HELLO, message, message_len);

    // TODO: not relative paths
    font_id font =
        font_initialize("../assets/fonts/AdwaitaSans-Regular.ttf", 32);
    texture_id tex = texture_initialize("../assets/images/test.png");

    while (update()) {
        clear_color(BLACK);

        pa_action player_action = (pa_action){
            .type = PA_NO_ACTION,
        };
        if (is_key_down(KEY_A)) {
            player_action.type = PA_MOVE_LEFT;
        }
        if (is_key_down(KEY_D)) {
            player_action.type = PA_MOVE_RIGHT;
        }
        if (is_mouse_button_pressed(MOUSE_LEFT)) {
            player_action.type = PA_SHOOT;

            i32 x, y;
            get_mouse_position(&x, &y);

            player_action.shoot.click_pos = (vec2){x, y};
        }

        net_pack *package = client_recv_data(&c);
        if (!package) {
            continue;
        }

        switch (package->type) {
        case NP_TYPE_DRAW_CMD: {
            render_snapshot snapshot;
            memcpy(&snapshot, package->data, package->size);
            printf("Snapthot count: %d\n", snapshot.count);

            for (i32 i = 0; i < snapshot.count; i++) {
                u32 w, h;
                get_window_size(&w, &h);

                draw_cmd *cmd = &snapshot.commands[i];

                // convert pos from top left origin to center to render
                vec2 pos = (vec2){
                    .x = cmd->pos.x + cmd->scale.x * 0.5f,
                    .y = cmd->pos.y + cmd->scale.y * 0.5f,
                };

                vec2 scale_v = (vec2){
                    .x = cmd->scale.x * 0.5f,
                    .y = cmd->scale.y * 0.5f,
                };

                switch (cmd->type) {
                case DRAW_CMD_TYPE_QUAD: {
                    matrix scale;
                    math_matrix_scale(&scale, scale_v.x, scale_v.y, 1.0);

                    matrix trans;
                    math_matrix_translate(&trans, pos.x, pos.y, 0.0f);

                    matrix model;
                    math_matrix_mul(&model, &trans, &scale);

                    matrix view;
                    math_matrix_get_orthographic(w, h, &view);

                    matrix m;
                    math_matrix_mul(&m, &view, &model);

                    draw_mesh(quad_vertices, 6, m.m, NO_TEXTURE,
                              cmd->quad.color);
                } break;
                case DRAW_CMD_TYPE_TEXTURE: {
                    matrix scale;
                    math_matrix_scale(&scale, scale_v.x, scale_v.y, 1.0);

                    matrix trans;
                    math_matrix_translate(&trans, pos.x, pos.y, 0.0f);

                    matrix model;
                    math_matrix_mul(&model, &trans, &scale);

                    matrix view;
                    math_matrix_get_orthographic(w, h, &view);

                    matrix m;
                    math_matrix_mul(&m, &view, &model);

                    draw_mesh(quad_vertices, 6, m.m, cmd->texture.id, WHITE);
                } break;
                case DRAW_CMD_TYPE_TEXT: {
                    draw_text(cmd->text.chars, cmd->pos.x, cmd->pos.y,
                              cmd->text.font, cmd->scale.x, cmd->text.color);
                } break;
                }
            }
            break;
        }
        }

        client_send_data(&c, NP_TYPE_PA, &player_action, sizeof(player_action));

        free(package);
        swap_buffers();
    }

    texture_deinitialize(tex);
    font_deinitialize(font);

    deinitialize_client(&c);
    quit();

    return 0;
}
