#include <X11/X.h>
#include <X11/Xlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "types.h"

#define BUF_SIZE 1024

static Display *display;
static Atom wm_delete_window;

static u32 window_width = 1280;
static u32 window_height = 720;

void get_window_size(u32 *w, u32 *h) {
    *w = window_width;
    *h = window_height;
}

bool update(void) {
    XEvent event;

    while (XPending(display)) {
        XNextEvent(display, &event);

        switch (event.type) {
        case ClientMessage: {
            if ((Atom)event.xclient.data.l[0] == wm_delete_window) {
                return false;
            }
        } break;
        case ConfigureNotify: {
            window_width = event.xconfigure.width;
            window_height = event.xconfigure.height;
        } break;
        }
    }

    return true;
}

int main(int argc, char **argv) {
    display = XOpenDisplay(NULL);

    if (!display) {
        fprintf(stderr, "cannot open display\n");

        return 1;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    Window window = XCreateSimpleWindow(display, root, 0, 0, 1280, 720, 2,
                                        BlackPixel(display, screen),
                                        WhitePixel(display, screen));
    XStoreName(display, window, "Game");
    XSelectInput(display, window,
                 ExposureMask | KeyPressMask | StructureNotifyMask);

    // handle polite close request so we are not force-killed by the window
    // manager
    wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    XMapWindow(display, window);

    char *ip = "127.0.0.1";
    if (argc > 1) {
        ip = argv[1];
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        fprintf(stderr, "socket: %s\n", strerror(errno));

        return 1;
    }

    {
        // set recv timeout so we do not block forever if the server does not
        // respond
        struct timeval tv = {
            .tv_sec = 5,
            .tv_usec = 0,
        };
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
            fprintf(stderr, "setsockopt: %s\n", strerror(errno));
        }
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

    unsigned char buf[BUF_SIZE];
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len = sizeof(recv_addr);
    memset(&recv_addr, 0, sizeof(recv_addr));

    ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                         (struct sockaddr *)&recv_addr, &recv_addr_len);

    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "recvfrom: timeout waiting for response\n");
        } else {
            fprintf(stderr, "recvfrom: %s\n", strerror(errno));
        }

        close(sock);

        return 1;
    }

    if (recv_addr.sin_addr.s_addr != server_addr.sin_addr.s_addr ||
        recv_addr.sin_port != server_addr.sin_port) {
        fprintf(stderr, "recvfrom: response came from an unexpected source");

        close(sock);

        return 1;
    }

    buf[n] = '\0';
    printf("recv %zd bytes: %s\n", n, buf);

    while (update()) {
    }

    close(sock);

    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}
