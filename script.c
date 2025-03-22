#include "engine/skeletal_mesh.h"
#include "engine/shader.h"
#include "engine/our_gl.h"
#include "engine/model.h"

#include "engine/serialize/serialize_skm.h"

#include "obj/shader.h"

#include <cglm/cglm.h>

struct skeletal_mesh test_mesh = {0};
struct skm_armature_anim test_anim = {0};
struct skm_armature_anim_playback test_playback = {0};

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

static GLuint v_loc = 0;
static GLuint p_loc = 0;
//static GLuint m_loc = 0;
static GLuint skeleton_loc = 0;
static GLuint skel_shader = 0;

// Will eventually be replaced by skeleton stuff.
static mat4 m_matrix;

static GLuint skeleton_count_loc = 0;


static mat4 skeleton_matrix[4];

// view projection matrix
static mat4 p_matrix;

static mat4 v_matrix;

void
setup_proj_mat(float window_w, float window_h) {
    // Eventually, we will want all sorts of nonsense, but for now, let's create
    // a simple perspective matrix..?
    glm_perspective(90.0f, window_w / window_h, 0.01f, 100.0f, p_matrix);
}

void
pass_vp() {
    REPORT(glUniformMatrix4fv(p_loc, 1, false, p_matrix[0]));
    REPORT(glUniformMatrix4fv(v_loc, 1, false, v_matrix[0]));
}

void
window_resized_hook(int width, int height) {
    setup_proj_mat(width, height);
    pass_vp();
}

#define DIST_FROM_CAM 8

void
init() {
    skel_shader = ourgl_compile_shader(skel_vert_src, skel_frag_src);

    REPORT(p_loc = glGetUniformLocation(skel_shader, "u_p"));
    REPORT(v_loc = glGetUniformLocation(skel_shader, "u_v"));

    REPORT(skeleton_loc = glGetUniformLocation(skel_shader, "u_skeleton"));
    REPORT(skeleton_count_loc = glGetUniformLocation(skel_shader, "u_skeleton_count"));

    glm_mat4_identity(m_matrix);

    const float w = 640;
    const float h = 480;
    setup_proj_mat(w, h);

    REPORT(glUseProgram(skel_shader));
    glm_mat4_identity(v_matrix);
    glm_rotated(v_matrix, 0.3, (vec3){ 1.0, 0.0, 0.0 });
    glm_translated(v_matrix, (vec3){ 0.0, 0.0, -8.0 });
    pass_vp();

    struct skeletal_mesh *wanted_meshes[] = { &test_mesh };

    struct skm_armature_anim *wanted_anim[] = { &test_anim };

    struct import_data id = {
        .skm = wanted_meshes,
        .num_skm = 1,
        .got_skm = 0,

        .skm_arm_anim = wanted_anim,
        .num_skm_arm_anim = 1,
        .got_skm_arm_anim = 0,
    };

    load_model("blender/player.glb", &id);

    skm_arm_playback_init(&test_playback, &test_anim);

    test_mesh.shader = skel_shader;
    skm_gl_init(&test_mesh);

    REPORT(glUniform1f(skeleton_count_loc, (float)test_mesh.bone_count));
    REPORT(glUniform1i(skeleton_loc, 0));

    SDL_Log("init called.");
}

#include <stdlib.h>

static bool dump = true;
void
dump_mat(const char *name, mat4 mat);

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
tick(double dt) {
    skm_arm_playback_apply(&test_playback);
    skm_compute_matrices(&test_mesh, m_matrix);
    skm_arm_playback_step(&test_playback, dt);
    if(test_playback.time >= 1.25) {
        skm_arm_playback_seek(&test_playback, test_playback.time - 1.25);
    }

    // The world-space position of each bone should be something like:
    // model matrix * bone matrix * inverse bind matrix * position

    for(int i = 0; i < test_mesh.bone_count; ++i) {
        mat4 final_transform;
        //if(dump) dump_mat("bone pose", test_mesh.bone_pose[i]);
        //if(dump) dump_mat("bone bind", test_mesh.bone_inverse_bind[i]);
        glm_mat4_mul(test_mesh.bone_pose[i], test_mesh.bone_inverse_bind[i], final_transform);
        //glm_mat4_copy(final_transform, skeleton_matrix[i]);

        skm_set_bone_global_transform(&test_mesh, i, final_transform);
        
    }

    skm_gl_upload_bone_tform(&test_mesh);

    skm_gl_draw(&test_mesh);
}