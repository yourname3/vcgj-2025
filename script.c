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

// const char *my_vs=
// "#version 100\n"
// "precision highp float;\n"
// "\n"
// "attribute vec3 a_pos;\n"
// "attribute vec3 a_norm;\n"
// "\n"
// "varying vec3 v_norm;\n"
// "\n"
// "uniform mat4 u_vp;\n"
// "uniform mat4 u_m;\n"
// "\n"
// "void main() {\n"
// "    v_norm = (u_m * vec4(a_norm, 0.0)).xyz; // cheap approx\n"
// "    gl_Position = u_vp * u_m * vec4(a_pos, 1.0);\n"
// "}\n";

// const char *my_fs=
// "#version 100\n"
// "precision highp float;\n"
// "\n"
// "varying vec3 v_norm;\n"
// "\n"
// "void main() {\n"
// "    vec3 norm = normalize(v_norm);\n"
// "    float light = clamp(dot(norm, normalize(vec3(-0.5, -0.5, 1.0))), 0.0, 1.0);\n"
// "    light += 0.2;\n"
// "    light = clamp(light, 0.0, 1.0);\n"
// "    vec3 color = light * vec3(1.0, 0.5, 0.2);"
// "    gl_FragColor = vec4(color, 1.0);\n"
// "}\n";

static GLuint vp_loc = 0;
//static GLuint m_loc = 0;
static GLuint skeleton_loc = 0;
static GLuint skel_shader = 0;

// Will eventually be replaced by skeleton stuff.
static mat4 m_matrix;


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
    mat4 vp;
    glm_mat4_mul(p_matrix, v_matrix, vp);

    REPORT(glUniformMatrix4fv(vp_loc, 1, false, vp[0]));
}

#define DIST_FROM_CAM 8

void
init() {
    skel_shader = ourgl_compile_shader(skel_vert_src, skel_frag_src);

    REPORT(vp_loc = glGetUniformLocation(skel_shader, "u_vp"));
    //REPORT(m_loc = glGetUniformLocation(skel_shader, "u_m"));
    REPORT(skeleton_loc = glGetUniformLocation(skel_shader, "u_skeleton"));

    //mat4 model_pose;
    glm_mat4_identity(m_matrix);
   // 
    

    const float w = 640;
    const float h = 480;
    setup_proj_mat(w, h);
    m_matrix[3][2] -= DIST_FROM_CAM;

    REPORT(glUseProgram(skel_shader));
    glm_mat4_identity(v_matrix);
    pass_vp();
    //glUniformMatrix4fv(m_loc, 1, false, m_matrix[0]);
    //glm_ortho(-w / 2.0, w / 2.0, -h / 2.0, h / 2.0, -4096.0, 4096.0, vp_matrix);



    //skm_init(&test_mesh, vertices, 12, triangles, 6, shader);

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

    

    load_model("blender/test_model_1.glb", &id);

    struct serializer *s = get_stdio_writer("test.skm");
    write_skm(s, &test_mesh);
    close_stdio_write(s);

    skm_arm_playback_init(&test_playback, &test_anim);

    test_mesh.shader = skel_shader;
    skm_gl_init(&test_mesh);

    //REPORT(glDisable(GL_DEPTH_TEST));
    //REPORT(glDisable(GL_CULL_FACE));

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
    
    // vec3 back = { 0.0, 0.0, DIST_FROM_CAM };
    // vec3 forth = { 0.0, 0.0, -DIST_FROM_CAM };
    // glm_translated(m_matrix, back);
    // vec3 up = { 0.0, 1.0, 0.0 };
    // glm_rotated(m_matrix, 0.00003f, up);
    // glm_translated(m_matrix, forth);

   // vec3 weird = { 0.0, 0.0, 1.0 };
   // janky_rotate(test_mesh.bone_local_pose[1], 0.00003f, weird);
    //janky_rotate(test_mesh.bone_local_pose[2], 0.00003f, weird);

    //janky_rotate(test_mesh.bone_local_pose[1], 0.00003f, test_mesh.bone_local_pose[1][2]);

    skm_arm_playback_apply(&test_playback);
    skm_compute_matrices(&test_mesh, m_matrix);
    skm_arm_playback_step(&test_playback, dt);
    if(test_playback.time > 2.0) {
        skm_arm_playback_seek(&test_playback, 0.0);
    }

    // The world-space position of each bone should be something like:
    // model matrix * bone matrix * inverse bind matrix * position

    for(int i = 0; i < test_mesh.bone_count; ++i) {
        mat4 final_transform;
        //if(dump) dump_mat("bone pose", test_mesh.bone_pose[i]);
        //if(dump) dump_mat("bone bind", test_mesh.bone_inverse_bind[i]);
        glm_mat4_mul(test_mesh.bone_pose[i], test_mesh.bone_inverse_bind[i], final_transform);
        glm_mat4_copy(final_transform, skeleton_matrix[i]);

        //if(dump) dump_mat("final transform", final_transform);
        
    }
    dump = false;

    // glm_mat4_copy(m_matrix, skeleton_matrix[0]);
    // glm_mat4_copy(m_matrix, skeleton_matrix[1]);
    // glm_mat4_copy(m_matrix, skeleton_matrix[2]);
    // glm_mat4_copy(m_matrix, skeleton_matrix[3]);

    glUniformMatrix4fv(skeleton_loc, 4, false, skeleton_matrix[0][0]);
    //pass_vp();

    skm_gl_draw(&test_mesh);
}