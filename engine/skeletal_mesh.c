#include "skeletal_mesh.h"

#include "types.h"
#include "alloc.h"

#include "our_gl.h"

void
skm_init(struct skeletal_mesh *skm, float *vertices, size_t vertices_count, GLuint *triangles, size_t triangles_count, GLuint shader) {
    const size_t bytes = vertices_count * sizeof(*vertices);

    skm->vertices = eng_zalloc(bytes);
    memcpy(skm->vertices, vertices, bytes);

    skm->vertices_count = vertices_count;

    const size_t tbytes = triangles_count * sizeof(*triangles);
    skm->triangles = eng_zalloc(tbytes);
    memcpy(skm->triangles, triangles, tbytes);
    skm->triangles_count = triangles_count;

    skm->array_buf = 0;
    skm->shader = shader;
}

/**
 * Creates the GL state necessary to render the given skeletal mesh.
 */
void
skm_gl_init(struct skeletal_mesh *skm) {
    REPORT(glGenBuffers(1, &skm->array_buf));
    REPORT(glGenBuffers(1, &skm->element_buf));

    skm_gl_upload(skm);
}

/**
 * Re-uploads the vertex data for the given mesh.
 */
void
skm_gl_upload(struct skeletal_mesh *skm) {
    size_t bytes = sizeof(*skm->vertices) * skm->vertices_count;

    REPORT(glBindBuffer(GL_ARRAY_BUFFER, skm->array_buf));
    REPORT(glBufferData(GL_ARRAY_BUFFER, bytes, skm->vertices, GL_STATIC_DRAW));

    size_t tbytes = sizeof(*skm->triangles) * skm->triangles_count;

    REPORT(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skm->element_buf));
    REPORT(glBufferData(GL_ELEMENT_ARRAY_BUFFER, tbytes, skm->triangles, GL_STATIC_DRAW));
}

/**
 * Draws the skeletal mesh using opengl.
 */
void
skm_gl_draw(struct skeletal_mesh *skm) {
    REPORT(glBindBuffer(GL_ARRAY_BUFFER, skm->array_buf));

    REPORT(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)0));
    REPORT(glEnableVertexAttribArray(0));

    REPORT(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(sizeof(float) * 3)));
    REPORT(glEnableVertexAttribArray(1));

    REPORT(glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(sizeof(float) * 6)));
    REPORT(glEnableVertexAttribArray(2));

    REPORT(glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(sizeof(float) * 10)));
    REPORT(glEnableVertexAttribArray(3));

    REPORT(glUseProgram(skm->shader));

    REPORT(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skm->element_buf));
    REPORT(glDrawElements(GL_TRIANGLES, skm->triangles_count, GL_UNSIGNED_INT, 0));
}

void
skm_destroy(struct skeletal_mesh *skm) {
    eng_free(skm->vertices, skm->vertices_count * sizeof(*skm->vertices));
    skm->vertices = NULL;
    skm->vertices_count = 0;
    
    // TODO destroy GL properties
}

static bool dump = true;
void
dump_mat(const char *name, mat4 mat);

static void
skm_compute_dfs(struct skeletal_mesh *skm, int idx, mat4 root_pose) {
    if(idx == -1) return;

    if(skm->bone_compute_graph[idx]) return;

    skm->bone_compute_graph[idx] = true;

    mat4 parent_matrix;
    int parent_idx = skm->bone_heirarchy[idx];
    
    if(parent_idx < 0) {
        glm_mat4_copy(root_pose, parent_matrix);
    }
    else {
        //if(dump) SDL_Log("bone %d: parent = %d\n", idx, parent_idx);
        skm_compute_dfs(skm, parent_idx, root_pose);
        
        glm_mat4_copy(skm->bone_pose[parent_idx], parent_matrix);
        //if(dump) dump_mat("source parent pose", skm->bone_pose[parent_idx]);
        //if(dump) dump_mat("computed parent pose", parent_matrix);
    }

    //if(dump) SDL_Log("bone %d: compute pose !!!", idx);
    //if(dump) dump_mat("input 1: ", parent_matrix);
    //if(dump) dump_mat("input 2: ", skm->bone_local_pose[idx]); 
    glm_mat4_mul(parent_matrix, skm->bone_local_pose[idx], skm->bone_pose[idx]);
    //if(dump) dump_mat("i computed my pose: ", skm->bone_pose[idx]);
}



void
skm_compute_matrices(struct skeletal_mesh *skm, mat4 root_pose) {
    // Reset the graph
    for(size_t i = 0; i < skm->bone_count; ++i) {
        skm->bone_compute_graph[i] = false;
    }

    for(size_t i = 0; i < skm->bone_count; ++i) {
        skm_compute_dfs(skm, i, root_pose);
    }
}

void
skm_arm_bone_lerp_keys(struct skm_arm_anim_bone_playback *state, struct skm_arm_anim_bone *keys, float time) {
    struct skm_vec3_key *p0 = &keys->position[state->position_idx];
    struct skm_vec3_key *p1 = p0;
    if(state->position_idx + 1 < keys->position_count) {
        p1 = &keys->position[state->position_idx + 1];
    }

    float t = 0.0;

    if(p0 != p1) { t = (time - p0->time) / (p1->time - p0->time); }
    

    vec3 pos;
    glm_vec3_lerp(p0->value, p1->value, t, pos);

    p0 = &keys->scale[state->scale_idx];
    p1 = p0;
    if(state->scale_idx + 1 < keys->scale_count) {
        p1 = &keys->scale[state->scale_idx + 1];
    }

    t = 0.0;
    if(p0 != p1) { t = (time - p0->time) / (p1->time - p0->time); }

    vec3 scale;
    glm_vec3_lerp(p0->value, p1->value, t, scale);

    struct skm_quat_key *q0 = &keys->rotation[state->rotation_idx];
    struct skm_quat_key *q1 = q0;
    if(state->rotation_idx + 1 < keys->rotation_count) {
        q1 = &keys->rotation[state->rotation_idx + 1];
    }

    t = 0.0;
    if(q0 != q1) { t = (time - q0->time) / (q1->time - q0->time); }

    //SDL_Log("fractional t = %f\n", t);
    //SDL_Log("diff = %f vs %f\n",q0->time, q1->time);
    

    versor rot;
    glm_quat_nlerp(q0->value, q1->value, t, rot);

    glm_translate_make(state->position_matrix, pos);
    glm_quat_mat4(rot, state->rotation_matrix);
    glm_scale_make(state->scale_matrix, scale);

    // dump_mat("translate", state->position_matrix);
    // dump_mat("rotate", state->rotation_matrix);
    // dump_mat("scale", state->scale_matrix);
}

void
skm_arm_bone_seek(struct skm_arm_anim_bone_playback *state, struct skm_arm_anim_bone *keys, float time) {
    state->position_idx = 0;
    state->rotation_idx = 0;
    state->scale_idx = 0;

    while(state->position_idx + 1 < keys->position_count) {
        if(keys->position[state->position_idx + 1].time > time) {
            break;
        }

        state->position_idx += 1;
    }

    while(state->rotation_idx + 1 < keys->rotation_count) {
        if(keys->rotation[state->rotation_idx + 1].time > time) {
            SDL_Log("seek: checked %d keys", state->rotation_idx);
            break;
        }

        state->rotation_idx += 1;
    }

    while(state->scale_idx + 1 < keys->scale_count) {
        if(keys->scale[state->scale_idx + 1].time > time) {
            break;
        }

        state->scale_idx += 1;
    }

    skm_arm_bone_lerp_keys(state, keys, time);
}

void
skm_arm_playback_seek(struct skm_armature_anim_playback *playback, float time) {
    SDL_Log("--- begin seek ---");
    for(size_t i = 0; i < playback->anim->skm->bone_count; ++i) {
        skm_arm_bone_seek(&playback->state[i], &playback->anim->bones[i], time);
    }
    playback->time = time;
}

void
skm_arm_playback_init(struct skm_armature_anim_playback *playback, struct skm_armature_anim *anim) {
    playback->anim = anim;
    playback->state = eng_zalloc(sizeof(*playback->state) * anim->skm->bone_count);
    skm_arm_playback_seek(playback, 0.0f);
}

void
skm_arm_playback_apply(struct skm_armature_anim_playback *playback) {
    struct skeletal_mesh *skm = playback->anim->skm;

    for(size_t i = 0; i < skm->bone_count; ++i) {
        glm_mat4_copy(playback->state[i].scale_matrix, skm->bone_local_pose[i]);
        glm_mat4_mul(playback->state[i].rotation_matrix, skm->bone_local_pose[i], skm->bone_local_pose[i]);
        glm_mat4_mul(playback->state[i].position_matrix, skm->bone_local_pose[i], skm->bone_local_pose[i]);
    }
}

void
skm_arm_playback_step(struct skm_armature_anim_playback *playback, float step) {
    // TODO: Make this not absurdly inefficient.
    skm_arm_playback_seek(playback, playback->time + step);
}