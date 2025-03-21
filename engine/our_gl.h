#ifndef ENGINE_GL_H
#define ENGINE_GL_H

#ifdef __EMSCRIPTEN__
#include <GLES2/gl2.h>
#else
#include <glad/glad.h>
#endif

#include <SDL3/SDL_log.h>

#include <stdlib.h>

static inline const char*
ourgl_error_string(GLuint err) {
    switch(err) {
        case GL_NO_ERROR: return "no error";
        case GL_INVALID_ENUM: return "invalid enum";
        case GL_INVALID_VALUE: return "invalid value";
        case GL_INVALID_OPERATION: return "invalid operation";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "invalid framebuffer operation";
        case GL_OUT_OF_MEMORY: return "out of memory";
    #ifdef GL_STACK_UNDERFLOW
        case GL_STACK_UNDERFLOW: return "stack underflow";
    #endif
    #ifdef GL_STACK_OVERFLOW
        case GL_STACK_OVERFLOW: return "stack overflow";
    #endif
    }
    return "unknown";
}

static inline int
ourgl_report(const char *file, int lineno, const char *line) {
    GLuint err = 0;
    int had_err = 0;
    for(;;) {
        err = glGetError();

        if(err == GL_NO_ERROR) return had_err;

        SDL_Log("GL error: %s:%d: during %s:\n\terror code %u: %s",
            file, lineno, line, err, ourgl_error_string(err));

        // Exit on OpenGL error so we can see it.
        exit(1);

        had_err = 1;
    }
}

#define REPORT(...) \
__VA_ARGS__ ; \
ourgl_report(__FILE__, __LINE__, #__VA_ARGS__) 

#define REPORT_OR_ZERO(...) __VA_ARGS__ ; \
if(ourgl_report(__FILE__, __LINE__, #__VA_ARGS__)) { return 0; }

#endif