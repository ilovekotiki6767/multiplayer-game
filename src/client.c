#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

int main(int argc, char **argv) {
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
        // set recv timeout so we do not block forever if the server does not respond
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

    ssize_t sent = sendto(sock, message, message_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

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

    ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&recv_addr, &recv_addr_len);

    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "recvfrom: timeout waiting for response\n");
        } else {
            fprintf(stderr, "recvfrom: %s\n", strerror(errno));
        }

        close(sock);

        return 1;
    }

    if (recv_addr.sin_addr.s_addr != server_addr.sin_addr.s_addr || recv_addr.sin_port != server_addr.sin_port) {
        fprintf(stderr, "recvfrom: response came from an unexpected source");

        close(sock);

        return 1;
    }

    buf[n] = '\0';
    printf("recv %zd bytes: %s\n", n, buf);

    close(sock);

    return 0;
}
