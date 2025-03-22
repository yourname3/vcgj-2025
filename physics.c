#include "map.h"
#include "engine/types.h"

uint8_t
map_get(struct map *map, int32_t x, int32_t y) {
    if(x < 0 || y < 0) return CELL_EMPTY;
    if(x >= map->width || y >= map->height) return CELL_EMPTY;

    y = map->height - y - 1;

    return map->data[y * map->width + x];
}

void
map_set(struct map *map, int32_t x, int32_t y, uint8_t value) {
    if(x < 0 || y < 0) return;
    if(x >= map->width || y >= map->height) return;

    y = map->height - y - 1;

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

//
// MORE LORE: it turns out that the 6th index is required to get ANYTHING
// to draw. So the 1st block is drawn, the 6th, etc, but this depends on as
// we iterate through the map. So 5/6th blocks are just rejected.

uint8_t map0_data[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 0, 0, 0, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
};

struct map map0 = {
    .width = 8,
    .height = 5,

    .player_x = 5,
    .player_y = 3, // Assuming we flip the coords..?

    .data = map0_data
};