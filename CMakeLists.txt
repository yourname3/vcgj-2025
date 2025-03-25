cmake_minimum_required(VERSION 3.10)

project(bens-bales)

# find_package(SDL3 REQUIRED)
# find_package(SDL3_mixer REQUIRED)
# find_package(assimp REQUIRED)
include(FetchContent)
FetchContent_Declare(
    SDL3 
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        f6864924f76e1a0b4abaefc76ae2ed22b1a8916e
)

FetchContent_Declare(
    SDL3_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG        cadfafb61e03fc38ce8aac849de4b892f880bb24
)

FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        c35200e38ea8f058812b83de2ef32c6093b0ece2
)

FetchContent_Declare(
    cglm
    GIT_REPOSITORY https://github.com/recp/cglm.git
    GIT_TAG        144d1e7c29b3b0c6dede7917a0476cc95248559c
)

# --- Configure our FetchContent dependencies ---
# We're using INTERNAL for all these options as in theory we don't want the user
# of our game build system to modify them?

# Build SDL as static library. That's what we're going to want for game
# distribution (if we ever figure out how to pack the assets).
set(SDL_STATIC   ON  CACHE INTERNAL "")
set(SDL_SHARED   OFF CACHE INTERNAL "")

# Subsystems we definitely don't need
set(SDL_GPU      OFF CACHE INTERNAL "")
set(SDL_RENDER   OFF CACHE INTERNAL "")
# Subsystems that we don't need for this game
set(SDL_CAMERA   OFF CACHE INTERNAL "")
set(SDL_JOYSTICK OFF CACHE INTERNAL "")
set(SDL_HAPTIC   OFF CACHE INTERNAL "")
set(SDL_POWER    OFF CACHE INTERNAL "")
set(SDL_SENSOR   OFF CACHE INTERNAL "")
set(SDL_DIALOG   OFF CACHE INTERNAL "")

# Don't build shared libraries for SDL3_mixer
# (and I guess for our project too)
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

# Setup desired audio formats
set(SDLMIXER_FLAC    OFF CACHE INTERNAL "")
set(SDLMIXER_GME     OFF CACHE INTERNAL "")
set(SDLMIXER_MIDI    OFF CACHE INTERNAL "")
set(SDLMIXER_MOD     OFF CACHE INTERNAL "")
set(SDLMIXER_MP3     OFF CACHE INTERNAL "")
set(SDLMIXER_OPUS    OFF CACHE INTERNAL "")
# Use stb vorbis, I guess
set(SDLMIXER_VORBIS  STB CACHE INTERNAL "")
set(SDLMIXER_WAVE    ON  CACHE INTERNAL "")
set(SDLMIXER_WAVPACK OFF CACHE INTERNAL "")

# Assimp options
# TODO: Figure out a way to make it only build the gltf importer?
set(ASSIMP_BUILD_TESTS        OFF CACHE INTERNAL "")

# Have assimp build own zlib
set(ASSIMP_BUILD_ZLIB         ON CACHE INTERNAL "")

# We can't have this enabled or assimp won't build.
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE INTERNAL "")

# Only import GLTF files. No others are needed.
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE INTERNAL "")

# Cglm options
set(CGLM_SHARED OFF CACHE INTERNAL "")
set(CGLM_STATIC ON  CACHE INTERNAL "")

FetchContent_MakeAvailable(SDL3 SDL3_mixer assimp cglm)

add_executable(shader2c tool/shader2c.c)

set(SHADERS
    shader/static-vert.glsl
    shader/skel-vert.glsl
    shader/skel-frag.glsl
)

list(TRANSFORM SHADERS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)

list(TRANSFORM ASSETS REPLACE ".+" "--preload-file ${CMAKE_CURRENT_SOURCE_DIR}\\0@\\0")
separate_arguments(ASSETS)

add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/shader.c" "${CMAKE_BINARY_DIR}/shader.h"
    COMMAND shader2c ${SHADERS} "${CMAKE_BINARY_DIR}/shader.c" "${CMAKE_BINARY_DIR}/shader.h"
    DEPENDS ${SHADERS}
    COMMENT "Generate shaders as C code"
)

add_executable(bens-bales
    nuklear.c
    physics.c
    script.c
    engine/serialize/serialize.c
    engine/serialize/skm_serialize.c
    engine/main.c
    engine/model.c
    engine/shader.c
    engine/skeletal_mesh.c
    engine/stb_image.c
    glad/src/glad.c
    "${CMAKE_BINARY_DIR}/shader.c"
)

target_include_directories(bens-bales PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/glad/include")

target_link_libraries(bens-bales PRIVATE SDL3::SDL3-static)
target_link_libraries(bens-bales PRIVATE SDL3_mixer::SDL3_mixer)
target_link_libraries(bens-bales PRIVATE assimp::assimp)
target_link_libraries(bens-bales PRIVATE cglm)

if(EMSCRIPTEN)
    target_compile_options(bens-bales PRIVATE "-fexceptions")
    target_link_options(shader2c PRIVATE "-sFORCE_FILESYSTEM=1" "-lnoderawfs.js" "-lnodefs.js")
    target_link_options(bens-bales PRIVATE ${ASSETS})
    target_link_options(bens-bales PRIVATE
        "-sNO_DISABLE_EXCEPTION_CATCHING"
        "-sALLOW_MEMORY_GROWTH"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    )

    set_target_properties(bens-bales
        PROPERTIES
            SUFFIX ".html")
endif()