
#ifndef COMMON_H
#define COMMON_H

#include "cmath.h"
#include "types.h"

// player actions
enum {
    PA_NO_ACTION,
    PA_MOVE_LEFT,
    PA_MOVE_RIGHT,
    PA_SHOOT,
};

// provide additional info to player action which are needed
// like cursor position etc..
typedef struct {
    i32 type;

    union {
        struct {
            vec2 click_pos;
        } shoot;
    };
} pa_action;

#endif // COMMON_H
