#ifndef ENGINE_ALLOC_H
#define ENGINE_ALLOC_H

#include <stdlib.h>

#include <SDL3/SDL_log.h>

static inline void*
eng_zalloc(size_t bytes) {
    void *result = calloc(bytes, 1);
    if(!result) {
        SDL_Log("fatal: could not allocate");
        exit(1);
    }
    return result;
}

static inline void
eng_free(void *ptr, size_t size) {
    free(ptr);
}

#define malloc PLEASE_USE_eng_zalloc
#define calloc PLEASE_USE_eng_zalloc
#define free   PLEASE_USE_eng_free

#endif