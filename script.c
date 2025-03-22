#include "engine/skeletal_mesh.h"
#include "engine/shader.h"
#include "engine/our_gl.h"
#include "engine/model.h"
#include "engine/alloc.h"

#include "engine/serialize/serialize_skm.h"

#include "obj/shader.h"

#include "map.h"

#include <cglm/cglm.h>

struct skeletal_mesh player_mesh = {0};
struct skm_armature_anim player_walk_anim = {0};
struct skm_armature_anim_playback player_walk_playback = {0};

struct skeletal_mesh hay_mesh = {0};

// Notes to self:
// Uniforms maintain their values per-shader even after we bind a different shader.
// We only need to set uniforms that actually have changed. (Maybe cache things
// like textures in that case).
//
// If we want to do something like a global uniform, we would basically want to
// have the same uniform in every shader, and then do something like set it only
// when it has changed. So like, whenever we glUseProgram() a specific shader,
// or whatever, we would have a "uniform sync" step that checked the wanted state
// for the shader versus the last stored state, and update everything that needed
// updating.

struct {
    GLuint v; // view matrix
    GLuint p; // projection matrix
    GLuint skeleton; // skeleton texture bind
    GLuint skeleton_count; // skeleton bone count (float)

    GLuint base_color;
    GLuint metallic;
    GLuint perceptual_roughness;

    GLuint self; // the shader program
} skel_pbr;

// projection matrix
static mat4 p_matrix;

// view matrix
static mat4 v_matrix;

struct {
    mat4 model_matrix;
} player;

struct {
    GLuint v;
    GLuint p;
    GLuint m;

    GLuint base_color;
    GLuint metallic;
    GLuint perceptual_roughness;

    GLuint self;
} static_pbr;

struct {
    GLuint array_buf;
    GLuint element_buf;

    size_t triangle_count;

    GLuint shader;
} level_mesh = {0};

void
init_level_gl() {
    REPORT(glGenBuffers(1, &level_mesh.array_buf));
    REPORT(glGenBuffers(1, &level_mesh.element_buf));
}

void
copy_hay_mesh(float *verts, GLuint *tris, GLuint *vertptr, GLuint *triptr, size_t vert_data_count,
        size_t tri_data_count, int x, int y) {
    
    // The actual vertex indices into the array, as far as the GPU is concerned,
    // are real vertex indices, i.e. the sub_data pointer divided by 6.
    GLuint tri_base = *vertptr / 6;

    SDL_Log("copy a mesh to %d %d -> tribase = %lu", x, y, tri_base);
    SDL_Log("triptr = %lu", *triptr);

    float off_x = x * 2;
    float off_y = y * 2;

    for(size_t i = 0; i < vert_data_count; ++i) {
        size_t i6 = *vertptr;
        size_t i14 = i * 14;
        verts[i6 + 0] = hay_mesh.vertices[i14 + 0] + off_x;
        verts[i6 + 1] = hay_mesh.vertices[i14 + 1] + off_y;
        verts[i6 + 2] = hay_mesh.vertices[i14 + 2];
        verts[i6 + 3] = hay_mesh.vertices[i14 + 3];
        verts[i6 + 4] = hay_mesh.vertices[i14 + 4];
        verts[i6 + 5] = hay_mesh.vertices[i14 + 5];

        *vertptr += 6;
    }

    for(size_t i = 0; i < tri_data_count; ++i) {
        size_t j3 = *triptr;
        size_t i3 = i * 3;
        tris[j3 + 0] = hay_mesh.triangles[i3 + 0] + tri_base;
        tris[j3 + 1] = hay_mesh.triangles[i3 + 1] + tri_base;
        tris[j3 + 2] = hay_mesh.triangles[i3 + 2] + tri_base;

        *triptr += 3;
    }
}

void
gen_level_mesh(struct map *map) {
    // First pass: count mesh.
    size_t hay_count = 0;

    for(int x = 0; x < map->width; ++x) {
        for(int y = 0; y < map->height; ++y) {
            if(map_get(map, x, y) == CELL_HAY) {
                hay_count += 1;
            }
        }
    }

    size_t vert_data_count = hay_mesh.vertices_count / 14;
    size_t tri_data_count = hay_mesh.triangles_count / 3;

    SDL_Log("vertdc: %llu, tridc: %llu", vert_data_count, tri_data_count);

    // Fow now, just clone the vertex data for every vertex. We could try to 
    // find a way to only have one copy of normals.
    size_t verts_size = sizeof(float) * 6 * hay_count * vert_data_count;
    float *verts = eng_zalloc(verts_size);
    size_t tris_size = sizeof(GLuint) * 3 * hay_count * tri_data_count;
    GLuint *tris = eng_zalloc(tris_size);

    GLuint vertptr = 0;
    GLuint triptr = 0;

    level_mesh.triangle_count = hay_count * tri_data_count * 3;

    SDL_Log("level_mesh.triangle_count = %llu", level_mesh.triangle_count);

    for(int x = 0; x < map->width; ++x) {
        for(int y = 0; y < map->height; ++y) {
            if(map_get(map, x, y) == CELL_HAY) {
                copy_hay_mesh(verts, tris, &vertptr, &triptr, vert_data_count,
                    tri_data_count, x, y);
            }
        }
    }

    REPORT(glBindBuffer(GL_ARRAY_BUFFER, level_mesh.array_buf));
    REPORT(glBufferData(GL_ARRAY_BUFFER, verts_size, verts, GL_STATIC_DRAW));
    REPORT(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, level_mesh.element_buf));
    REPORT(glBufferData(GL_ELEMENT_ARRAY_BUFFER, tris_size, tris, GL_STATIC_DRAW));

    eng_free(verts, verts_size);
    eng_free(tris, tris_size);
}

void
setup_proj_mat(float window_w, float window_h) {
    // Eventually, we will want all sorts of nonsense, but for now, let's create
    // a simple perspective matrix..?
    // 22 = human eyesight ish?
    glm_perspective(70.0f, window_w / window_h, 0.01f, 100.0f, p_matrix);
}

void
pass_vp() {
    // Update the projection/view in the skel_pbr
    REPORT(glUseProgram(skel_pbr.self));
    REPORT(glUniformMatrix4fv(skel_pbr.p, 1, false, p_matrix[0]));
    REPORT(glUniformMatrix4fv(skel_pbr.v, 1, false, v_matrix[0]));

    REPORT(glUseProgram(static_pbr.self));
    REPORT(glUniformMatrix4fv(static_pbr.p, 1, false, p_matrix[0]));
    REPORT(glUniformMatrix4fv(static_pbr.v, 1, false, v_matrix[0]));
}

void
window_resized_hook(int width, int height) {
    setup_proj_mat(width, height);
    pass_vp();
}

#define DIST_FROM_CAM 8

void
init() {
    static_pbr.self = ourgl_compile_shader(static_vert_src, skel_frag_src);
    REPORT(static_pbr.p = glGetUniformLocation(static_pbr.self, "u_p"));
    REPORT(static_pbr.v = glGetUniformLocation(static_pbr.self, "u_v"));
    REPORT(static_pbr.m = glGetUniformLocation(static_pbr.self, "u_m"));
    REPORT(static_pbr.metallic = glGetUniformLocation(static_pbr.self, "metallic"));
    REPORT(static_pbr.perceptual_roughness = glGetUniformLocation(static_pbr.self, "perceptual_roughness"));
    REPORT(static_pbr.base_color = glGetUniformLocation(static_pbr.self, "base_color"));

    skel_pbr.self = ourgl_compile_shader(skel_vert_src, skel_frag_src);

    REPORT(skel_pbr.p = glGetUniformLocation(skel_pbr.self, "u_p"));
    REPORT(skel_pbr.v = glGetUniformLocation(skel_pbr.self, "u_v"));

    REPORT(skel_pbr.skeleton = glGetUniformLocation(skel_pbr.self, "u_skeleton"));
    REPORT(skel_pbr.skeleton_count = glGetUniformLocation(skel_pbr.self, "u_skeleton_count"));

    REPORT(skel_pbr.metallic = glGetUniformLocation(skel_pbr.self, "metallic"));
    REPORT(skel_pbr.perceptual_roughness = glGetUniformLocation(skel_pbr.self, "perceptual_roughness"));
    REPORT(skel_pbr.base_color = glGetUniformLocation(skel_pbr.self, "base_color"));

    const float w = 640;
    const float h = 480;
    setup_proj_mat(w, h);

    mat4 level_tform;
    glm_mat4_identity(level_tform);

    REPORT(glUseProgram(static_pbr.self));
    REPORT(glUniformMatrix4fv(static_pbr.m, 1, false, level_tform[0]));

    REPORT(glUseProgram(skel_pbr.self));
    glm_mat4_identity(v_matrix);
    glm_rotated(v_matrix, 0.3, (vec3){ 1.0, 0.0, 0.0 });
    glm_translated(v_matrix, (vec3){ 0.0, 0.0, -DIST_FROM_CAM });
    pass_vp();

    struct import_data player_id = {
        .skm = (struct skeletal_mesh*[]){ &player_mesh },
        .num_skm = 1,
        .got_skm = 0,

        .skm_arm_anim = (struct skm_armature_anim*[]){ &player_walk_anim },
        .num_skm_arm_anim = 1,
        .got_skm_arm_anim = 0,
    };

    struct import_data hay_id = {
        .skm = (struct skeletal_mesh*[]){ &hay_mesh },
        .num_skm = 1,
        .got_skm = 0,

        .skm_arm_anim = NULL,
        .num_skm_arm_anim = 0,
        .got_skm_arm_anim = 0,
    };

    load_model("blender/player.glb", &player_id);
    load_model("blender/hay.glb", &hay_id);

    skm_arm_playback_init(&player_walk_playback, &player_walk_anim);

    player_mesh.shader = skel_pbr.self;
    skm_gl_init(&player_mesh);

    REPORT(glUseProgram(skel_pbr.self));
    REPORT(glUniform1f(skel_pbr.skeleton_count, (float)player_mesh.bone_count));
    REPORT(glUniform1i(skel_pbr.skeleton, 0));

    SDL_Log("init called.");

    init_level_gl();
    gen_level_mesh(&map0);
}

#include <stdlib.h>

void
janky_rotate(mat4 pose, float amount, vec3 axis) {
    vec3 translate;
    glm_vec3_copy(pose[3], translate);
    translate[0] *= -1;
    translate[1] *= -1;
    translate[2] *= -1;
    glm_translated(pose, translate);
    glm_rotated(pose, amount, axis);
    translate[0] *= -1;
    translate[1] *= -1;
    translate[2] *= -1;
    glm_translated(pose, translate);
}

void
tick_player(double dt) {
    // Reset the player's matrix.
    glm_mat4_identity(player.model_matrix);
    glm_translated(player.model_matrix, (vec3){ 4.0, 2.0, 0.0 });

    // Set up camera.
    glm_mat4_identity(v_matrix);
    // Negative player translation..
    // Invert translation.
    glm_rotated(v_matrix, 0.3, (vec3){ 1.0, 0.0, 0.0 });
    glm_translated(v_matrix, (vec3){ 0.0, 0.0, -DIST_FROM_CAM });
    // focus on player
    glm_translated(v_matrix, (vec3){ -4.0, -2.0, 0.0 });
    
    pass_vp();
}

void
tick(double dt) {
    tick_player(dt);

    skm_arm_playback_apply(&player_walk_playback);
    skm_compute_matrices(&player_mesh, player.model_matrix);
    skm_arm_playback_step(&player_walk_playback, dt);
    if(player_walk_playback.time >= 1.25) {
        skm_arm_playback_seek(&player_walk_playback, player_walk_playback.time - 1.25);
    }

    // The world-space position of each bone should be something like:
    // model matrix * bone matrix * inverse bind matrix * position

    for(int i = 0; i < player_mesh.bone_count; ++i) {
        mat4 final_transform;

        glm_mat4_mul(player_mesh.bone_pose[i], player_mesh.bone_inverse_bind[i], final_transform);

        skm_set_bone_global_transform(&player_mesh, i, final_transform);
        
    }

    skm_gl_upload_bone_tform(&player_mesh);
}

void
render() {
    REPORT(glUseProgram(skel_pbr.self));
    glUniform1f(skel_pbr.metallic, 0.2);
    glUniform1f(skel_pbr.perceptual_roughness, 0.3);
    glUniform3f(skel_pbr.base_color, 0.5, 0.2, 0.8);
    skm_gl_draw(&player_mesh);

    REPORT(glBindBuffer(GL_ARRAY_BUFFER, level_mesh.array_buf));
    REPORT(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, level_mesh.element_buf));

    REPORT(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0));
    REPORT(glEnableVertexAttribArray(0));

    REPORT(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(sizeof(float) * 3)));
    REPORT(glEnableVertexAttribArray(1));

    //SDL_Log("level mesh tri count: %u\n", level_mesh.triangle_count);

    REPORT(glUseProgram(static_pbr.self));
    glUniform1f(static_pbr.metallic, 0.0);
    glUniform1f(static_pbr.perceptual_roughness, 0.95);
    glUniform3f(static_pbr.base_color, 246.0/255.0, 247.0/255.0, 146.0/255.0);

    REPORT(glDrawElements(GL_TRIANGLES, level_mesh.triangle_count, GL_UNSIGNED_INT, 0));
}