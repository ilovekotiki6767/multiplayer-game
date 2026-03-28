#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "cmath.h"
#include "collision.h"
#include "common.h"
#include "net.h"
#include "renderer.h"

typedef struct {
    vec2 pos;
    vec2 vel;
    vec2 acc;
} player;

// in seconds
static f32 get_time(u0) {
    static f64 start = 0.0;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    f64 now = ts.tv_sec + ts.tv_nsec * 1e-9;
    if (start == 0.0) {
        start = now;
    }

    return now - start;
}

i32 main(void) {
    net_server s;
    if (!initialize_server(&s, SERVER_PORT)) {
        fprintf(stderr, "failed starting server\n");
        return 1;
    }

    fprintf(stderr, "started server successfuly\n");

    const vec2 gravity = VEC2(0, 250);

    player players[MAX_CLIENTS];
    i32 player_idx = 0;

    f32 last_time = get_time();
    render_snapshot snaphshot = {0};

    while (true) {
        f32 current_time = get_time();
        f32 dt = current_time - last_time;
        last_time = current_time;

        snaphshot.count = 0;

        net_peer peer;

        // allocation need to be freed
        net_pack *package = server_recv_data(&s, &peer);
        if (!package) {
            continue;
        }

        switch (package->type) {
        case NP_TYPE_HELLO: {
            i32 handle = server_add_peer(&s, &peer, player_idx);
            players[player_idx++] = (player){
                .pos = {350, -100},
            };

            printf("Client [%d/%d] recv %s:%d: %.*s\n", handle + 1, MAX_CLIENTS,
                   peer.ip, ntohs(peer.addr.sin_port), package->size,
                   package->data);
        } break;

        case NP_TYPE_PA: {
            pa_action player_action = *(pa_action *)package->data;
            player *p = &players[peer.pid];

            if (player_action.type == PA_MOVE_LEFT) {
                p->vel.x -= 100 * dt;
            }
            if (player_action.type == PA_MOVE_RIGHT) {
                p->vel.x += 100 * dt;
            }
            if (player_action.type == PA_SHOOT) {
                vec2 pos = player_action.shoot.click_pos;

                vec2 delta = math_vec2_scale(
                    math_vec2_norm(math_vec2_subtract(p->pos, pos)), 175.0f);
                p->vel = math_vec2_add(p->vel, delta);
            }

        } break;
        }

        // update clients
        for (i32 i = 0; i < player_idx; i++) {
            player *p = &players[i];

            // right now only force acting is gravity
            p->acc = gravity;

            p->vel = math_vec2_add(p->vel, math_vec2_scale(p->acc, dt));
            p->pos = math_vec2_add(p->pos, math_vec2_scale(p->vel, dt));

            p->vel = math_vec2_scale(p->vel, 0.999f);
            p->acc = VEC2(0, 0);

            // collisions
            vec2 buttom_pos = VEC2(p->pos.x, p->pos.y + 15.0f);
            if (coll_check_point_rect(buttom_pos, VEC2(250, 400),
                                      VEC2(500, 50))) {
                p->pos.y = 400.0f - 15.0f;
                p->vel.y = 0.0f;
            }
        }

        draw_cmd ground = (draw_cmd){
            .type = DRAW_CMD_TYPE_QUAD,
            .pos = {250, 400},
            .scale = {500.0f, 50.0f},
            .quad.color = GREEN,
        };

        snaphshot.commands[snaphshot.count++] = ground;

        // draw commands for every client
        for (i32 i = 0; i < player_idx; i++) {
            // player body
            draw_cmd cmd = (draw_cmd){
                .type = DRAW_CMD_TYPE_QUAD,
                .pos = players[i].pos,
                .scale = {15.0f, 15.0f},
                .quad.color = RED,
            };

            snaphshot.commands[snaphshot.count++] = cmd;
        }

        // send to clients
        for (i32 i = 0; i < player_idx; i++) {
            server_send_data(&s, i, NP_TYPE_DRAW_CMD, &snaphshot,
                             sizeof(i32) + snaphshot.count * sizeof(draw_cmd));
        }

        free(package);
    }

    deinitialize_server(&s);

    return 0;
}
