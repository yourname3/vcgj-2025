#include <stdint.h>
#include <stdbool.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "our_gl.h"
#include "../actions.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

__attribute__((dllexport)) uint32_t NvOptimusEnablement = 0x00000001;  

extern void init();
extern void tick(double dt);
extern void render();

static SDL_Window *window = NULL;
SDL_GLContext gl_ctx = NULL;

static bool quit = false;

uint64_t ticks = 0;

#define TICKS_WINDOW_SIZE 32

uint64_t ticks_window[TICKS_WINDOW_SIZE] = { 0 };
uint64_t ticks_sum = 0;
size_t   ticks_window_ptr = 0;

// This time is added as we move farther into the game. We subtract from it
// when we tick.
int64_t time_in_future = 0;

void
ticks_push(uint64_t value) {
    size_t next = (ticks_window_ptr + 1) % TICKS_WINDOW_SIZE;
    ticks_sum -= ticks_window[next];
    ticks_sum += value;

    ticks_window[next] = value;
    ticks_window_ptr = next;
}

void
finalize() {
    SDL_Log("Shutting down.");
}

extern void window_resized_hook(int width, int height);

void
window_resized(int width, int height) {
    REPORT(glViewport(0, 0, width, height));
    window_resized_hook(width, height);
}

void
act_update(struct action *act, SDL_Keycode key, bool pressed) {
    if(key == act->code) {
        act->is_pressed = pressed;
    }
}

void
act_tick(struct action *act) {
    act->was_pressed = act->is_pressed;
}

bool
act_just_pressed(struct action *act) {
    return act->is_pressed && !act->was_pressed;
}

void
main_loop(void) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                window_resized(event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_KEY_DOWN:
                act_update(&act_left, event.key.key, true);
                act_update(&act_right, event.key.key, true);
                act_update(&act_jump, event.key.key, true);
                break;
            case SDL_EVENT_KEY_UP:
                act_update(&act_left, event.key.key, false);
                act_update(&act_right, event.key.key, false);
                act_update(&act_jump, event.key.key, false);
                break;
        }
    }

#ifdef __EMSCRIPTEN__
    if(quit) {
        emscripten_cancel_main_loop();
        finalize();
        return;
    }
#endif

    glClearColor(0.1, 0.8, 0.8, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //uint64_t dt_estimate = ticks_sum / TICKS_WINDOW_SIZE;

    //double dt = (double)dt_estimate / (double)SDL_GetPerformanceFrequency();
    //if(rand() % 256 == 0) SDL_Log("estimated fps: %f", 1.0 / dt);

    double dt_wanted = 1.0 / 60.0;
    int64_t step = (int64_t)(dt_wanted * (double)SDL_GetPerformanceFrequency());

    if(time_in_future > step * 8) {
        time_in_future = step * 8;
    }

    while(time_in_future > step) {
        tick(dt_wanted);
        /* After each frame, update keys. TODO: Maybe re-check inputs for each tick? */
        act_tick(&act_left);
        act_tick(&act_right);
        act_tick(&act_jump);
        time_in_future -= step;
    }

    render();
    SDL_GL_SwapWindow(window);

    uint64_t next_ticks = SDL_GetPerformanceCounter();
    uint64_t delta = next_ticks - ticks;
    ticks = next_ticks;

    time_in_future += (int64_t)delta;

    ticks_push(delta);
}

#define APP_TITLE "app title"
#define APP_VERSION "1.0"
#define APP_IDENTIFIER "com.example.app"

int
main(int argc, char **argv) {
    SDL_SetAppMetadata(APP_TITLE, APP_VERSION, APP_IDENTIFIER);

    SDL_Log("Initializing.");

    if(!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initial SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    // Compatibility renderer needed for things like no VAO
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    window = SDL_CreateWindow(APP_TITLE, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if(!window) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return 1;
    }

    gl_ctx = SDL_GL_CreateContext(window);
    if(!gl_ctx) {
        SDL_Log("Couldn't create OpenGL context: %s", SDL_GetError());
        return 1;
    }

    gladLoadGLLoader((GLADloadproc) SDL_GL_GetProcAddress);
    SDL_Log("OpenGL version: %d.%d", GLVersion.major, GLVersion.minor);

    const GLubyte* vendor = glGetString(GL_VENDOR); // Returns the vendor
    const GLubyte* renderer = glGetString(GL_RENDERER); // Returns a hint to the model
    SDL_Log("                %s %s", vendor, renderer);

    REPORT(glViewport(0, 0, 640, 480));
    REPORT(glEnable(GL_CULL_FACE));
    REPORT(glEnable(GL_DEPTH_TEST));

    SDL_GL_SetSwapInterval(1);

    init();
    ticks = SDL_GetPerformanceCounter();

    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, 1);
    #else
    while(!quit) {
        main_loop();
    }

    finalize();
    #endif

    return 0;
}