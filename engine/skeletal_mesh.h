#ifndef ENGINE_SKELETAL_MESH_H
#define ENGINE_SKELETAL_MESH_H

#include "our_gl.h"
#include <cglm/cglm.h>

struct skeletal_mesh {
    float *vertices;
    size_t vertices_count;

    uint32_t *triangles;
    size_t triangles_count;

    GLuint shader;

    GLuint bone_tform_tex;
    float *bone_tform_tex_data;

    GLuint array_buf;
    GLuint element_buf;

    mat4 *bone_inverse_bind;
    mat4 *bone_pose;
    mat4 *bone_local_pose;

    bool *bone_compute_graph;

    int32_t *bone_heirarchy;

    size_t bone_count;

    void *import_key;
};

struct skm_vec3_key {
    float time;
    vec3 value;
};

struct skm_quat_key {
    float time;
    versor value;
};

struct skm_arm_anim_bone {
    struct skm_vec3_key *position;
    size_t position_count;
    struct skm_vec3_key *scale;
    size_t scale_count;
    struct skm_quat_key *rotation;
    size_t rotation_count;
};

struct skm_armature_anim {
    struct skeletal_mesh *skm;

    float length;

    /* Should match bone_count */
    struct skm_arm_anim_bone *bones;
};

struct skm_arm_anim_bone_playback {
    size_t position_idx;
    size_t scale_idx;
    size_t rotation_idx;

    // Just store the various matrices directly in our playback state. That way,
    // for any channel with only a single key, we can simply never update the
    // matrix.
    mat4 position_matrix;
    mat4 scale_matrix;
    mat4 rotation_matrix;
};

struct skm_armature_anim_playback {
    struct skm_armature_anim *anim;

    struct skm_arm_anim_bone_playback *state;
    float time;
};

void skm_init(struct skeletal_mesh *skm, float *vertices, size_t vertices_count, GLuint *triangles, size_t triangles_count, GLuint shader);

/**
 * Creates the GL state necessary to render the given skeletal mesh.
 */
void skm_gl_init(struct skeletal_mesh *skm);

/**
 * Re-uploads the vertex data for the given mesh.
 */
void skm_gl_upload(struct skeletal_mesh *skm);

/**
 * Draws the skeletal mesh using opengl.
 */
void skm_gl_draw(struct skeletal_mesh *skm);

void skm_destroy(struct skeletal_mesh *skm);

void skm_compute_matrices(struct skeletal_mesh *skm, mat4 root_pose);

void skm_arm_playback_init(struct skm_armature_anim_playback *playback, struct skm_armature_anim *anim);

void skm_arm_playback_apply(struct skm_armature_anim_playback *playback);

void skm_arm_playback_step(struct skm_armature_anim_playback *playback, float step);

void skm_arm_playback_seek(struct skm_armature_anim_playback *playback, float time);

/** 
 * Updates the bone_tform_tex_data with the given matrisx. 
 */
void skm_set_bone_global_transform(struct skeletal_mesh *skm, int index, mat4 tform);

/**
 * Uploads the current bone_tform_tex_data to opengl.
 */
void skm_gl_upload_bone_tform(struct skeletal_mesh *skm);

#endif