
#ifndef COLLISION_H
#define COLLISION_H

#include "cmath.h"

static inline bool coll_check_point_circle(vec2 p, vec2 center, f32 radius) {
    f32 dist = math_vec2_distance(p, center);
    if (dist >= radius)
        return false;
    return true;
}

static inline bool coll_check_point_rect(vec2 p, vec2 pos, vec2 size) {
    return (p.x > pos.x && p.x < pos.x + size.x && p.y > pos.y &&
            p.y < pos.y + size.y);
}

#endif // COLLISION_H
