#ifndef MAP_H
#define MAP_H

#include "engine/types.h"

#include <cglm/cglm.h>

struct map {
    int32_t width;
    int32_t height;

    int32_t player_x;
    int32_t player_y;

    uint8_t *data;
};

struct phys_obj {
    vec2 pos;
    bool on_floor;

    vec2 col_normals[4];
    int32_t col_normal_count;
};

struct overlap {
    bool is_overlap;

    struct phys_obj owned;
    struct phys_obj *collision;
};

uint8_t map_get(struct map *map, int32_t x, int32_t y);

void map_set(struct map *map, int32_t x, int32_t y, uint8_t value);

void
phys_slide_motion_solver(vec2 vel, vec2 vel_out, struct phys_obj *obj, float margin, float dt);

extern struct map map0;

#define CELL_EMPTY 0
#define CELL_HAY   1
#define CELL_PLAYER 2

#endif