#ifndef ENG_MODEL_H
#define ENG_MODEL_H

#include "skeletal_mesh.h"

struct import_data {
    struct skeletal_mesh **skm;
    size_t num_skm;
    size_t got_skm;

    struct skm_armature_anim **skm_arm_anim;
    size_t num_skm_arm_anim;
    size_t got_skm_arm_anim;

    GLuint *texture;
    size_t num_texture;
    size_t got_texture;
};

// How many things we put in each vertex.
#define SKEL_MESH_4BYTES_COUNT 16

void load_model(const char *path, struct import_data *id);

#endif