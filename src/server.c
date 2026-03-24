#include <stdio.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define MAX_CLIENTS 32

typedef struct {
    struct sockaddr_in address;
    bool alive;
} client;

int main(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        fprintf(stderr, "socket: %s\n", strerror(errno));

        return 1;
    }

    {
        int opt = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
            -1) {
            fprintf(stderr, "setsockopt: %s\n", strerror(errno));

            close(sock);

            return 1;
        }
    }

    {
        // cap the receieve buffer so it does not eat all of the kernel memory
        // just in case
        int rcvbuf = 256 * 1024;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) ==
            -1) {
            fprintf(stderr, "setsockopt: %s\n", strerror(errno));
        }
    }

    struct sockaddr_in bind_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(27015),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) == -1) {
        fprintf(stderr, "bind: %s\n", strerror(errno));

        close(sock);

        return 1;
    }

    client clients[MAX_CLIENTS];
    int client_idx = 0;

    while (true) {
        unsigned char buf[BUF_SIZE];
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        memset(&client_addr, 0, sizeof(client_addr));

        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                             (struct sockaddr *)&client_addr, &client_addr_len);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "recvfrom: %s\n", strerror(errno));

            continue;
        }

        if (client_addr.sin_family != AF_INET) {
            fprintf(stderr, "recvfrom: unexpected address family");

            continue;
        }

        char ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip)) == NULL) {

            fprintf(stderr, "inet_ntop: %s\n", strerror(errno));

            snprintf(ip, sizeof(ip), "?.?.?.?");
        }

        if (client_idx < MAX_CLIENTS) {
            clients[client_idx++] = (client){
                .address = client_addr,
                .alive = true,
            };
        }

        for (int i = 0; i < client_idx; i++) {
            struct sockaddr_in addr = clients[i].address;

            ssize_t sent = sendto(sock, buf, (size_t)n, 0,
                                  (struct sockaddr *)&addr, sizeof(addr));
            if (sent == -1) {
                if (errno == EINTR) {
                    continue;
                }

                fprintf(stderr, "sendto: %s\n", strerror(errno));

                continue;
            }

            if (sent != n) {
                fprintf(stderr, "sendto: short write\n");
                printf("sent %zd bytes to %s:%d\n", sent, ip,
                       ntohs(client_addr.sin_port));
            }
        }
    }

    close(sock);

    return 0;
}
