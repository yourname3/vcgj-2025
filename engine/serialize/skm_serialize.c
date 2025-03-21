#include "engine/serialize/serialize_skm.h"

void
write_skm(struct serializer *s, struct skeletal_mesh *skm) {
    write_float_array(s, skm->vertices_count, skm->vertices);
    write_u32_array(s, skm->triangles_count, skm->triangles);
}