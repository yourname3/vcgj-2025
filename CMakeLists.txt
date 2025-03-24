cmake_minimum_required(VERSION 3.10)

project(bens-bales)

find_package(SDL3 REQUIRED)
find_package(SDL3_mixer REQUIRED)
find_package(assimp REQUIRED)

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

