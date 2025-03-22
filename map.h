#ifndef MAP_H
#define MAP_H

#include "engine/types.h"

struct map {
    int32_t width;
    int32_t height;

    int32_t player_x;
    int32_t player_y;

    uint8_t *data;
};

uint8_t map_get(struct map *map, int32_t x, int32_t y);

void map_set(struct map *map, int32_t x, int32_t y, uint8_t value);

extern struct map map0;

#define CELL_EMPTY 0
#define CELL_HAY   1
#define CELL_PLAYER 2

#endif