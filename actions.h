#ifndef ACTIONS_H
#define ACTIONS_H

#include <SDL3/SDL_keycode.h>

struct action {
    SDL_Keycode code;
    bool is_pressed;
    bool was_pressed;
};

extern struct action act_left;
extern struct action act_right;
extern struct action act_jump;

bool act_just_pressed(struct action *act);

#endif