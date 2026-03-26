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

    i32 sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        fprintf(stderr, "socket: %s\n", strerror(errno));

        return 1;
    }

    const i32 opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    i32 flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(27015);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "inet_pton: invalid address\n");

        close(sock);

        return 1;
    }

    char *message = "hello";
    size_t message_len = strlen(message);

    ssize_t sent = sendto(sock, message, message_len, 0,
                          (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (sent == -1) {
        fprintf(stderr, "sendto: %s\n", strerror(errno));

        close(sock);

        return 1;
    }

    if ((size_t)sent != message_len) {
        fprintf(stderr, "sendto: short write\n");
    }

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

        struct sockaddr_in recv_addr;
        socklen_t recv_addr_len = sizeof(recv_addr);
        memset(&recv_addr, 0, sizeof(recv_addr));

        render_snapshot snapshot;
        ssize_t n = recvfrom(sock, &snapshot, sizeof(snapshot) - 1, 0,
                             (struct sockaddr *)&recv_addr, &recv_addr_len);
        if (n != -1) {
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
        }

        ssize_t sent =
            sendto(sock, &player_action, sizeof(player_action), 0,
                   (struct sockaddr *)&server_addr, sizeof(server_addr));

        swap_buffers();
    }

    texture_deinitialize(tex);
    font_deinitialize(font);

    close(sock);
    quit();

    return 0;
}
