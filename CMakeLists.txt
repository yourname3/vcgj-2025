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
    GIT_TAG        171eb2d420d5643e4ee11514a06e04a41a463bbd
)

FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        c35200e38ea8f058812b83de2ef32c6093b0ece2
)

# Build SDL as static library. That's what we're going to want for game
# distribution (if we ever figure out how to pack the assets).
set(SDL_STATIC   ON  CACHE INTERNAL)
set(SDL_SHARED   OFF CACHE INTERNAL)

# Subsystems we definitely don't need
set(SDL_GPU      OFF CACHE INTERNAL)
set(SDL_RENDER   OFF CACHE INTERNAL)
# Subsystems that we don't need for this game
set(SDL_CAMERA   OFF CACHE INTERNAL)
set(SDL_JOYSTICK OFF CACHE INTERNAL)
set(SDL_HAPTIC   OFF CACHE INTERNAL)
set(SDL_POWER    OFF CACHE INTERNAL)
set(SDL_SENSOR   OFF CACHE INTERNAL)
set(SDL_DIALOG   OFF CACHE INTERNAL)

# Don't build shared libraries for SDL3_mixer
# (and I guess for our project too)
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL)

# Setup desired audio formats
set(SDLMIXER_FLAC    OFF CACHE INTERNAL)
set(SDLMIXER_GME     OFF CACHE INTERNAL)
set(SDLMIXER_MIDI    OFF CACHE INTERNAL)
set(SDLMIXER_MOD     OFF CACHE INTERNAL)
set(SDLMIXER_MP3     OFF CACHE INTERNAL)
set(SDLMIXER_OPUS    OFF CACHE INTERNAL)
# Use stb vorbis, I guess
set(SDLMIXER_VORBIS  STB CACHE INTERNAL)
set(SDLMIXER_WAVE    ON  CACHE INTERNAL)
set(SDLMIXER_WAVPACK OFF CACHE INTERNAL)

# Assimp options
# TODO: Figure out a way to make it only build the gltf importer?
set(ASSIMP_BUILD_TESTS OFF CACHE INTERNAL)

# Have assimp build own zlib
set(ASSIMP_BUILD_ZLIB  ON CACHE INTERNAL)

FetchContent_MakeAvailable(SDL3 SDL3_mixer assimp)

add_executable(shader2c tool/shader2c.c)

set(SHADERS
    shader/static-vert.glsl
    shader/skel-vert.glsl
    shader/skel-frag.glsl
)

list(TRANSFORM SHADERS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)

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

