#include "map.h"
#include "engine/types.h"

uint8_t
map_get(struct map *map, int32_t x, int32_t y) {
    if(x < 0 || y < 0) return CELL_EMPTY;
    if(x >= map->width || y >= map->height) return CELL_EMPTY;

    return map->data[y * map->width + x];
}

void
map_set(struct map *map, int32_t x, int32_t y, uint8_t value) {
    if(x < 0 || y < 0) return;
    if(x >= map->width || y >= map->height) return;

    map->data[y * map->width + x] = value;
}

// uint8_t map0_data[] = {
//     0, 1, 1, 1, 1, 1, 1, 1,
//     0, 1, 1, 1, 1, 1, 1, 1,
//     0, 1, 1, 1, 1, 1, 1, 1,
//     0, 1, 1, 1, 1, 1, 1, 1,
//     0, 1, 1, 1, 1, 1, 1, 1
// };

// LORE:
// the 0th-5th are all 0,0
// the 6th is at correct place...?

uint8_t map0_data[] = {
    1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

struct map map0 = {
    .width = 8,
    .height = 5,

    .player_x = 5,
    .player_y = 3, // Assuming we flip the coords..?

    .data = map0_data
};