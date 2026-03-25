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
#include "platform.h"
#include "renderer.h"

#define BUF_SIZE 1024

// pos(3) uv(2)
static const f32 quad_vertices[] = {
    -1, -1, 0, 0, 0, 1, -1, 0, 1, 0, 1,  1, 0, 1, 1,
    -1, -1, 0, 0, 0, 1, 1,  0, 1, 1, -1, 1, 0, 0, 1,
};

int main(int argc, char **argv) {
    initialize();
    initialize_gl();

    char *ip = "127.0.0.1";
    if (argc > 1) {
        ip = argv[1];
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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

        struct sockaddr_in recv_addr;
        socklen_t recv_addr_len = sizeof(recv_addr);
        memset(&recv_addr, 0, sizeof(recv_addr));

        render_snapshot snapshot;
        ssize_t n = recvfrom(sock, &snapshot, sizeof(snapshot) - 1, 0,
                             (struct sockaddr *)&recv_addr, &recv_addr_len);
        if (n != -1) {
            for (int i = 0; i < snapshot.count; i++) {
                u32 w, h;
                get_window_size(&w, &h);

                draw_cmd *cmd = &snapshot.commands[i];

                switch (cmd->type) {
                case RENDER_OBJ_TYPE_QUAD: {
                    matrix scale;
                    math_matrix_scale(&scale, cmd->scale, cmd->scale, 1.0);

                    matrix trans;
                    math_matrix_translate(&trans, cmd->pos.x, cmd->pos.y, 0.0f);

                    matrix model;
                    math_matrix_mul(&model, &trans, &scale);

                    matrix view;
                    math_matrix_get_orthographic(w, h, &view);

                    matrix m;
                    math_matrix_mul(&m, &view, &model);

                    draw_mesh(quad_vertices, 6, m.m, NO_TEXTURE,
                              cmd->quad.color);
                } break;
                case RENDER_OBJ_TYPE_TEXTURE: {
                    matrix scale;
                    math_matrix_scale(&scale, cmd->scale, cmd->scale, 1.0);

                    matrix trans;
                    math_matrix_translate(&trans, cmd->pos.x, cmd->pos.y, 0.0f);

                    matrix model;
                    math_matrix_mul(&model, &trans, &scale);

                    matrix view;
                    math_matrix_get_orthographic(w, h, &view);

                    matrix m;
                    math_matrix_mul(&m, &view, &model);

                    draw_mesh(quad_vertices, 6, m.m, cmd->texture.id, WHITE);
                } break;
                case RENDER_OBJ_TYPE_TEXT: {
                    draw_text(cmd->text.chars, cmd->pos.x, cmd->pos.y,
                              cmd->text.font, cmd->scale, cmd->text.color);
                } break;
                }
            }
        }

        ssize_t sent =
            sendto(sock, message, message_len, 0,
                   (struct sockaddr *)&server_addr, sizeof(server_addr));

        swap_buffers();
    }

    texture_deinitialize(tex);
    font_deinitialize(font);

    close(sock);
    quit();

    return 0;
}
