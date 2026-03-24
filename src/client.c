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

    render_obj text = TEXT(VEC2(10, 0), 2.0f, "Yoo", font, RED);
    render_obj quad = QUAD(VEC2(0, 0), 100.0f, WHITE);
    render_obj texture = TEXTURE(VEC2(-100, 0), 150.0f, tex);

    while (update()) {
        unsigned char buf[BUF_SIZE];
        struct sockaddr_in recv_addr;
        socklen_t recv_addr_len = sizeof(recv_addr);
        memset(&recv_addr, 0, sizeof(recv_addr));

        ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr *)&recv_addr, &recv_addr_len);
        if (n != -1) {
            buf[n] = '\0';
            printf("recv %zd bytes: %s\n", n, buf);
        }
        clear_color(BLACK);

        // todo add depth (so they actually can overlap)
        draw_render_obj(&quad);
        draw_render_obj(&texture);
        draw_render_obj(&text);

        swap_buffers();
    }

    texture_deinitialize(tex);
    font_deinitialize(font);

    close(sock);
    quit();

    return 0;
}
