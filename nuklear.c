#define NK_IMPLEMENTATION
#define NK_SDL_GL3_IMPLEMENTATION
#define NK_SDL_GLES2_IMPLEMENTATION

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_IO

#include "engine/our_gl.h"

#include "Nuklear/nuklear.h"

#ifdef __EMSCRIPTEN__
    #include "nuklear_sdl3_gles2.h"
#else
    #include "nuklear_sdl3_gl3.h"
#endif