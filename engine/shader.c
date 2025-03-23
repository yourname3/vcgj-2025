#include "shader.h"

#include "our_gl.h"

#include <SDL3/SDL_log.h>

static int
err_log_compile(const char *kind, GLuint shader) {
    int success;
    /* TODO: Do this correctly */
    char info_log[512];
    REPORT(glGetShaderiv(shader, GL_COMPILE_STATUS, &success));

    if(!success) {
        REPORT(glGetShaderInfoLog(shader, 512, NULL, info_log));
        SDL_Log("Error compiling %s shader:\n%s", kind, info_log);
        return 1;
    }

    return 0;
}

static int
err_log_link(GLuint shader) {
    int success;
    /* TODO: Do this correctly */
    char info_log[512];
    REPORT(glGetProgramiv(shader, GL_LINK_STATUS, &success));

    if(!success) {
        REPORT(glGetProgramInfoLog(shader, 512, NULL, info_log));
        SDL_Log("Error linking shader:\n%s", info_log);
        return 1;
    }

    return 0;
}

GLuint
ourgl_compile_shader(const char *vs_src, const char *fs_src) {
    REPORT(GLuint vs = glCreateShader(GL_VERTEX_SHADER));
    REPORT(GLuint fs = glCreateShader(GL_FRAGMENT_SHADER));

    // TODO: Consider passing string length array to these.

    REPORT(glShaderSource(vs, 1, &vs_src, NULL));
    REPORT(glCompileShader(vs));

    if(err_log_compile("vertex", vs)) return 0;

    REPORT(glShaderSource(fs, 1, &fs_src, NULL));
    REPORT(glCompileShader(fs));

    if(err_log_compile("fragment", fs)) return 0;

    REPORT(GLuint shader = glCreateProgram());

    REPORT(glAttachShader(shader, vs));
    REPORT(glAttachShader(shader, fs));

    // Apparently this is necessary, either that or using glGetAttribLocation later.
    REPORT(glBindAttribLocation(shader, 0, "a_pos"));
    REPORT(glBindAttribLocation(shader, 1, "a_norm"));
    REPORT(glBindAttribLocation(shader, 2, "a_weight"));
    REPORT(glBindAttribLocation(shader, 3, "a_weight_idx"));
    REPORT(glBindAttribLocation(shader, 4, "a_uv"));
    

    REPORT(glLinkProgram(shader));

    SDL_Log("glGetAttribLocation(a_pos) -> %d\n", glGetAttribLocation(shader, "a_pos"));
    SDL_Log("glGetAttribLocation(a_norm) -> %d\n", glGetAttribLocation(shader, "a_norm"));

    if(err_log_link(shader)) return 0;

    REPORT(glDeleteShader(vs));
    REPORT(glDeleteShader(fs));

    return shader;
}