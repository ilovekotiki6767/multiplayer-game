
#include "net.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define HEADER_SIZE (sizeof(i32) * 2)

bool initialize_server(net_server *s, i32 port) {
    // create server socket
    s->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s->fd == -1) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        return false;
    }

    // enable reuse address
    const i32 opt = 1;
    setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt,
               sizeof(opt));

    // just in case cap the receive buffer
    i32 rcvbuf = 100 * UDP_MAX_SIZE;
    if (setsockopt(s->fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) ==
        -1) {
        fprintf(stderr, "setsockopt: %s\n", strerror(errno));
        return false;
    }

    // bind the server socket at the specified port
    struct sockaddr_in bind_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(s->fd, (const struct sockaddr *)&bind_addr, sizeof(bind_addr)) ==
        -1) {
        fprintf(stderr, "bind: %s\n", strerror(errno));
        close(s->fd);
        return false;
    }

    return true;
}

bool server_send_data(net_server *s, i32 peer, i32 type, const void *data,
                      u32 size) {
    // TODO: handle this better
    if (size > UDP_MAX_SIZE) {
        fprintf(stderr, "size too large max size: %u size: %u\n", UDP_MAX_SIZE,
                size);
        return false;
    }

    u32 total_size = sizeof(net_pack) + size;
    net_pack *p = malloc(total_size);

    if (!p) {
        fprintf(stderr, "alloc error when sending data to server\n");
        return false;
    }

    p->type = htons(type);
    p->size = htons(size);

    memcpy(p->data, data, size);

    ssize_t sent = sendto(s->fd, p, total_size, 0,
                          (const struct sockaddr *)&s->peers[peer].addr,
                          sizeof(s->peers[peer].addr));
    if (sent == -1) {
        if (errno == EINTR) {
            return false;
        }
        fprintf(stderr, "sendto: %s\n", strerror(errno));
        return false;
    }

    if ((u32)sent != total_size) {
        fprintf(stderr, "sendto: short write\n");
        printf("sent %zd bytes send instead of %u\n", sent, total_size);
    }

    free(p);

    return true;
}

net_pack *server_recv_data(net_server *s, net_peer *p) {
    memset(p, 0, sizeof(net_peer));

    // just to store the raw data
    u8 buffer[UDP_MAX_SIZE];

    // addr len will be discarded
    socklen_t addr_len = sizeof(p->addr);

    ssize_t n = recvfrom(s->fd, buffer, UDP_MAX_SIZE, 0,
                         (struct sockaddr *)&p->addr, &addr_len);
    if (n == -1) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            return NULL; // no data but no error
        }

        fprintf(stderr, "recvfrom: %s\n", strerror(errno));
        return NULL;
    }

    if (p->addr.sin_family != AF_INET) {
        fprintf(stderr, "recvfrom: unexpected address family");
        return NULL;
    }

    // convert ip to string
    if (inet_ntop(AF_INET, &p->addr.sin_addr, p->ip, sizeof(p->ip)) == NULL) {
        fprintf(stderr, "inet_ntop: %s\n", strerror(errno));
        snprintf(p->ip, sizeof(p->ip), "?.?.?.?");
    }

    p->id = p->addr.sin_port + p->addr.sin_addr.s_addr;

    // convert buffer to net_pack
    net_pack *package = malloc(n);
    memcpy(package, buffer, n);
    package->type = ntohs(package->type);
    package->size = ntohs(package->size);

    if (n != (i32)(package->size + HEADER_SIZE)) {
        fprintf(stderr, "Size mismatch recv: %zd but requested: %u\n", n,
                package->size);
        return NULL;
    }

    return package;
}

i32 server_add_peer(net_server *s, net_peer *p, i32 pid) {
    if (s->peers_idx >= MAX_CLIENTS) {
        return -1;
    }

    i32 handle = s->peers_idx;
    p->pid = pid;
    s->peers[s->peers_idx++] = *p;

    return handle;
}

u0 deinitialize_server(net_server *s) { close(s->fd); }

bool initialize_client(net_client *c, const char *address, i32 port) {
    memset(c, 0, sizeof(net_client));

    // create client socket
    c->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (c->fd == -1) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        return false;
    }

    // enable the reuse address for the client socket
    const i32 opt = 1;
    setsockopt(c->fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt,
               sizeof(opt));

    // make client socket non blocking
    i32 flags = fcntl(c->fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(c->fd, F_SETFL, flags | O_NONBLOCK);
    }

    // set server port
    c->server.sin_family = AF_INET;
    c->server.sin_port = htons(port);

    // set server ip
    if (inet_pton(AF_INET, address, &c->server.sin_addr) != 1) {
        fprintf(stderr, "inet_pton: invalid_address\n");
        close(c->fd);
        return false;
    }

    return true;
}

bool client_send_data(net_client *c, i32 type, const void *data, u32 size) {
    // TODO: handle this better
    if (size > UDP_MAX_SIZE) {
        fprintf(stderr, "size too large max size: %u size: %u\n", UDP_MAX_SIZE,
                size);
        return false;
    }

    u32 total_size = sizeof(net_pack) + size;
    net_pack *p = malloc(total_size);

    if (!p) {
        fprintf(stderr, "alloc error when sending data to server\n");
        return false;
    }

    p->type = htons(type);
    p->size = htons(size);

    memcpy(p->data, data, size);
    ssize_t sent =
        sendto(c->fd, p, total_size, 0, (const struct sockaddr *)&c->server,
               sizeof(c->server));

    if (sent == -1) {
        if (errno == EINTR) {
            return false;
        }
        fprintf(stderr, "sendto: %s\n", strerror(errno));
        return false;
    }

    if ((u32)sent != total_size) {
        fprintf(stderr, "sendto: short write\n");
        printf("sent %zd bytes send instead of %u\n", sent, total_size);
    }

    free(p);

    return true;
}

net_pack *client_recv_data(net_client *c) {
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len = sizeof(recv_addr);
    memset(&recv_addr, 0, sizeof(recv_addr));

    u8 buffer[UDP_MAX_SIZE];

    ssize_t n = recvfrom(c->fd, buffer, UDP_MAX_SIZE, 0,
                         (struct sockaddr *)&recv_addr, &recv_addr_len);
    if (n == -1) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            return NULL; // no data but no error
        }

        fprintf(stderr, "recvfrom: %s\n", strerror(errno));
        return NULL;
    }

    if (recv_addr.sin_family != AF_INET) {
        fprintf(stderr, "recvfrom: unexpected address family");
        return NULL;
    }

    // convert buffer to net_pack
    net_pack *package = malloc(n);
    memcpy(package, buffer, n);
    package->type = ntohs(package->type);
    package->size = ntohs(package->size);

    if (n != (i32)(package->size + HEADER_SIZE)) {
        fprintf(stderr, "Size mismatch recv: %zd but requested: %u\n", n,
                package->size);
        return NULL;
    }

    return package;
}

u0 deinitialize_client(net_client *c) { close(c->fd); }
