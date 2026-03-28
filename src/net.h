
#ifndef NET_H
#define NET_H

#include "types.h"
#include <netinet/in.h>

// cant be less then 5000 rn becasue a single render snaphsot is 4996 bytes
// without headeer
#define UDP_MAX_SIZE 5000u
#define MAX_CLIENTS 32

// pretty cooked desing rn
#define HEADER_SIZE (sizeof(i32) * 2)

#define SERVER_PORT 27015

// network packages types
enum {
    NP_TYPE_HELLO,
    NP_TYPE_PA,
    NP_TYPE_DRAW_CMD,
};

typedef struct __attribute__((packed)) {
    i32 type;
    u32 size;
    u8 data[];
} net_pack;

typedef struct {
    i32 fd;
    struct sockaddr_in server;
} net_client;

typedef struct {
    struct sockaddr_in addr;
    i32 id;  // unique to every peer
    i32 pid; // player handle

    // just for convenience
    char ip[INET_ADDRSTRLEN];
} net_peer;

typedef struct {
    i32 fd;

    i32 peers_idx;
    net_peer peers[MAX_CLIENTS];
} net_server;

bool initialize_server(net_server *s, i32 port);
bool server_send_data(net_server *s, i32 peer, i32 type, const void *data,
                      u32 size);
net_pack *server_recv_data(net_server *s, net_peer *p);
u0 deinitialize_server(net_server *s);

// returns handle to peer array in server
i32 server_add_peer(net_server *s, net_peer *p, i32 pid);

// returns the peer handle form the unique id
i32 server_get_peer_handle(net_server *s, i32 id);

// returns true on success
bool initialize_client(net_client *c, const char *address, i32 port);
bool client_send_data(net_client *c, i32 type, const void *data, u32 size);
net_pack *client_recv_data(net_client *c);
u0 deinitialize_client(net_client *c);

#endif // NET_H
