#include "map.h"
#include "engine/types.h"

#include <cglm/cglm.h>
#include <math.h>

#include <SDL3/SDL_log.h>

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
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    3, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 3, 0, 0, 0,
    1, 3, 0, 0, 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
};

struct map map0 = {
    .width = 21,
    .height = 9,

    .player_x = 5,
    .player_y = 3, // Assuming we flip the coords..?

    .data = map0_data
};

struct map *cur_map = &map0;

float
obj_left(struct phys_obj *obj) {
    return obj->pos[0] + obj->top_left[0];
}

float
obj_right(struct phys_obj *obj) {
    return obj->pos[0] + obj->bottom_right[0];
}

float
obj_bottom(struct phys_obj *obj) {
    return obj->pos[1] + obj->bottom_right[1];
}

float
obj_top(struct phys_obj *obj) {
    return obj->pos[1] + obj->top_left[1];
}

bool
obj_get_normal_from_overlap(vec2 normal_out, struct phys_obj *self, struct phys_obj *other) {
    glm_vec2_sub(self->pos, other->pos, normal_out);

    bool other_left_side = obj_left(other) <= obj_right(self);
    bool other_right_side = obj_right(other) <= obj_left(self);

    bool other_bottom_side = obj_bottom(other) <= obj_top(self);
    bool other_top_side = obj_top(other) <= obj_bottom(self);

    bool x_valid = (other_left_side == other_right_side);
    bool y_valid = (other_top_side == other_bottom_side);

    // If they're both valid,prefer y????
    //if(x_valid && y_valid) return false;

    if(y_valid) {
        normal_out[1] = (normal_out[1] > 0) ? 1 : -1;
        normal_out[0] = 0;
        return true;
    }

    if(x_valid) {
        normal_out[0] = (normal_out[0] > 0) ? 1 : -1;
        normal_out[1] = 0;
        return true;
    }

    // If neither are valid, it's completely inside us. We don't know which
    // direction.
    return false;
}

struct overlap
map_get_overlap_at_point(struct map *map, float x, float y) {
    int32_t cell_x = (int32_t)floorf((x + 1.0) / 2.0);
    int32_t cell_y = (int32_t)floorf((y + 1.0) / 2.0);
    //SDL_Log("check: %d, %d", cell_x, cell_y);
    uint8_t get = map_get(map, cell_x, cell_y);
    if(get == CELL_EMPTY) {
        //SDL_Log("empty celll!!");
        return (struct overlap){
            .is_overlap = false,
            .collision = NULL
        };
    }

    //SDL_Log("found cell at %d, %d ", cell_x, cell_y);

    // Construct a new phys_obj at that location.
    struct overlap result;
    result.is_overlap = true;
    result.collision = &result.owned;
    
    result.owned.col_normal_count = 0;
    result.owned.on_floor = false;

    // Should be right?
    result.owned.pos[0] = (float)cell_x * 2.0;
    result.owned.pos[1] = (float)cell_y * 2.0;
    
    result.owned.top_left[0] = -1;
    result.owned.top_left[1] = -1;

    result.owned.bottom_right[0] = 1;
    result.owned.bottom_right[1] = 1;

    return result;
}

struct overlap
map_get_overlap(struct map *map, struct phys_obj *obj) {
    struct overlap result = { .is_overlap = false, .collision = NULL };
    //SDL_Log("check tl");
    const float m = 0;
    result = map_get_overlap_at_point(map, obj_left(obj) + m, obj_top(obj) - m);
    if(result.is_overlap) return result;
    //SDL_Log("check bl");
    result = map_get_overlap_at_point(map, obj_left(obj) + m, obj_bottom(obj) + m);
    if(result.is_overlap) return result;
    //SDL_Log("check tr");
    result = map_get_overlap_at_point(map, obj_right(obj) - m, obj_top(obj) - m);
    if(result.is_overlap) return result;
    //SDL_Log("check br");
    result = map_get_overlap_at_point(map, obj_right(obj) - m, obj_bottom(obj) + m);
    if(result.is_overlap) return result;

    return result;
}

struct overlap
phys_has_any_overlap(struct phys_obj *obj) {
    return map_get_overlap(cur_map, obj);
}

struct overlap
phys_solve_motion_iterative(struct phys_obj *obj, vec2 motion, float margin) {
    vec2 motion_remain;
    glm_vec2_copy(motion, motion_remain);
    vec2 step;
    glm_vec2_copy(motion, step);

    struct overlap overlap = {
        .is_overlap = false,
        .collision = NULL
    };
    int iterations = 0;
    const int max_iterations = 80;

    while(glm_vec2_norm2(step) > margin && glm_vec2_norm2(motion_remain) > margin) {
        vec2 last_pos;
        glm_vec2_copy(obj->pos, last_pos);
        glm_vec2_add(obj->pos, step, obj->pos);

        overlap = phys_has_any_overlap(obj);
        if(overlap.is_overlap) {
            // retry with smaller step
            glm_vec2_scale(step, 0.5, step);
            glm_vec2_copy(last_pos, obj->pos);
        }
        else {
            glm_vec2_sub(motion_remain, step, motion_remain);
        }

        iterations += 1;
        if(iterations > max_iterations) break;
    }

    return overlap;
}

void
vec2_project(vec2 a, vec2 b, vec2 out) {
    float scale = glm_vec2_dot(a, b) / glm_vec2_dot(b, b);
    glm_vec2_scale(b, scale, out);
}

void
phys_slide_motion_solver(vec2 vel, vec2 vel_out, struct phys_obj *obj, float margin, float dt) {
    int max_slide_count = 2;

    vec2 total_vel;
    glm_vec2_scale(vel, dt, total_vel);

    glm_vec2_copy(vel, vel_out);

    // if(vel[0] > 0) {
    //     obj->on_floor = false;
    // }
    // if(vel[0] < -0.05) {
    //     obj->on_floor = false;
    // }
    obj->on_floor = false;
    obj->col_normal_count = 0;

    while(max_slide_count --> 0) {
        vec2 last_pos;
        glm_vec2_copy(obj->pos, last_pos);

        struct overlap overlap =
            phys_solve_motion_iterative(obj, total_vel, margin);

        //SDL_Log("overlap: %d", overlap.is_overlap);
        
        if(!overlap.is_overlap) {
            // No overlap--no collisions during this slide, early return.
            return;
        }

        // Otherwise, compute the normal vector.
        vec2 normal;
        if(!obj_get_normal_from_overlap(normal, obj, overlap.collision)) {
            // Couldn't get the normal--no overlap.
            return;
        }

        obj->col_normal_count++;
        // store the collision normal for reference.
        glm_vec2_copy(normal, obj->col_normals[obj->col_normal_count]);

        vec2 component;
        vec2_project(vel, normal, component);

        glm_vec2_sub(vel_out, component, vel_out);

        vec2 distance_moved;
        glm_vec2_sub(obj->pos, last_pos, distance_moved);

        // Subtract the distance moved off of the additional vel we need to
        // do. (total_vel should really be called "displacecment").
        glm_vec2_sub(total_vel, distance_moved, total_vel);

        vec2_project(total_vel, normal, component);
        // Also remove the collided component from the displacement.
        glm_vec2_sub(total_vel, component, total_vel);

        //SDL_Log("normal vector: %f %f", normal[0], normal[1]);

        // Set on floor property
        if(glm_vec2_dot(normal, (vec2){ 0, 1 }) > 0.5) {
            obj->on_floor = true;
        }
    }
}