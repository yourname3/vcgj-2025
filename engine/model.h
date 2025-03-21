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
};

void load_model(const char *path, struct import_data *id);

#endif