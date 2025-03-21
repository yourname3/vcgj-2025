#ifndef ENGINE_SERIALIZE_SKM_H
#define ENGINE_SERIALIZE_SKM_H

#include "engine/skeletal_mesh.h"
#include "engine/serialize/serialize.h"

void
write_skm(struct serializer *s, struct skeletal_mesh *skm);

#endif