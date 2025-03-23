#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "our_gl.h"

#include <SDL3/SDL_log.h>

GLuint
upload_embedded_texture(void *buf, size_t size) {
    int x, y, comp;
    uint8_t *data = stbi_load_from_memory(buf, size, &x, &y, &comp, 4);

    if(!data) {
        SDL_Log("Warning: Failed to load texture data.");
        return 0;
    }

    SDL_Log("Loaded texture data: %d %d %d", x, y, comp);
    
    GLuint tex;
    glGenTextures(1, &tex);

    REPORT(glBindTexture(GL_TEXTURE_2D, tex));
    REPORT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    REPORT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    REPORT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    REPORT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);

    return tex;
}