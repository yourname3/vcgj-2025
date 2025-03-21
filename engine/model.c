#include "model.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <SDL3/SDL_log.h>

#include "our_gl.h"
#include "alloc.h"

#include <assert.h>

struct import_mapping {
    struct aiString *name;
    struct aiNode *node;
    
    struct skeletal_mesh *skm;
    int skm_bone_idx;
};

struct import_mapping mappings[1024];
size_t mapping_count = 0xFFFFFFF;

void
convert_to_cglm(mat4 dest, struct aiMatrix4x4 *src) {
    dest[0][0] = src->a1;
    dest[0][1] = src->b1;
    dest[0][2] = src->c1;
    dest[0][3] = src->d1;

    dest[1][0] = src->a2;
    dest[1][1] = src->b2;
    dest[1][2] = src->c2;
    dest[1][3] = src->d2;

    dest[2][0] = src->a3;
    dest[2][1] = src->b3;
    dest[2][2] = src->c3;
    dest[2][3] = src->d3;

    dest[3][0] = src->a4;
    dest[3][1] = src->b4;
    dest[3][2] = src->c4;
    dest[3][3] = src->d4;
}

void
cvt_vec3(vec3 dest, struct aiVector3D *src) {
    dest[0] = src->x;
    dest[1] = src->y;
    dest[2] = src->z;
}

void
cvt_quat(versor dest, struct aiQuaternion *src) {
    dest[0] = src->x;
    dest[1] = src->y;
    dest[2] = src->z;
    dest[3] = src->w;
}

void
dump_mat(const char *name, mat4 mat) {
    SDL_Log("matrix %s: ---------", name);
    SDL_Log("  %10.4f %10.4f %10.4f %10.4f", mat[0][0], mat[1][0], mat[2][0], mat[3][0]);
    SDL_Log("  %10.4f %10.4f %10.4f %10.4f", mat[0][1], mat[1][1], mat[2][1], mat[3][1]);
    SDL_Log("  %10.4f %10.4f %10.4f %10.4f", mat[0][2], mat[1][2], mat[2][2], mat[3][2]);
    SDL_Log("  %10.4f %10.4f %10.4f %10.4f", mat[0][3], mat[1][3], mat[2][3], mat[3][3]);
    SDL_Log("--------------------");
}

void
handle_mesh(struct aiMesh *mesh, struct import_data *id) {
    SDL_Log("got mesh ");

    struct skeletal_mesh *output = NULL;
    if(id->got_skm < id->num_skm) {
        output = id->skm[id->got_skm++];
    }
    else return;

    float *vert_data;
    GLuint *elem_data;

    size_t vert_arrsize = mesh->mNumVertices * 14;
    size_t elem_arrsize = mesh->mNumFaces * 3; // Triangulated

    vert_data = eng_zalloc(sizeof(*vert_data) * vert_arrsize);
    elem_data = eng_zalloc(sizeof(*elem_data) * elem_arrsize);

    assert(mesh->mVertices);
    assert(mesh->mNormals);

    for(size_t i = 0; i < mesh->mNumVertices; ++i) {
        size_t i3 = i * 14;
        vert_data[i3 + 0] = mesh->mVertices[i].x;
        vert_data[i3 + 1] = mesh->mVertices[i].y;
        vert_data[i3 + 2] = mesh->mVertices[i].z;

        vert_data[i3 + 3] = mesh->mNormals[i].x;
        vert_data[i3 + 4] = mesh->mNormals[i].y;
        vert_data[i3 + 5] = mesh->mNormals[i].z;

        vert_data[i3 + 6] = 1.0;
        vert_data[i3 + 7] = 0.0;
        vert_data[i3 + 8] = 0.0;
        vert_data[i3 + 9] = 0.0;

        // Due to WebGL, we can't directly pass integers, we have to pass them
        // as floats and then cast them as integers.
        vert_data[i3 + 10] = -1;
        vert_data[i3 + 11] = -1;
        vert_data[i3 + 12] = -1;
        vert_data[i3 + 13] = -1;

        //SDL_Log("vert: %f %f %f\n", vert_data[i3 + 3], vert_data[i3 + 4], vert_data[i3 + 5]);
    }

    for(size_t i = 0; i < mesh->mNumFaces; ++i) {
        size_t i3 = i * 3;
        assert(mesh->mFaces[i].mNumIndices == 3);
        elem_data[i3 + 0] = mesh->mFaces[i].mIndices[0];
        elem_data[i3 + 1] = mesh->mFaces[i].mIndices[1];
        elem_data[i3 + 2] = mesh->mFaces[i].mIndices[2];
    }

    mat4 *inverse_bind = NULL;
    mat4 *pose = NULL;
    mat4 *local_pose = NULL;
    int *heirarchy = NULL;

    if(mesh->mBones) {
        struct aiNode **node_map = NULL;
        struct aiNode **tmp_heirarchy = NULL;

        SDL_Log("importing bones...");
        inverse_bind = eng_zalloc(sizeof(*inverse_bind) * mesh->mNumBones);
        pose = eng_zalloc(sizeof(*inverse_bind) * mesh->mNumBones);
        local_pose = eng_zalloc(sizeof(*inverse_bind) * mesh->mNumBones);
        heirarchy = eng_zalloc(sizeof(*heirarchy) * mesh->mNumBones);

        node_map = eng_zalloc(sizeof(*node_map) * mesh->mNumBones);
        tmp_heirarchy = eng_zalloc(sizeof(*tmp_heirarchy) * mesh->mNumBones);

        for(size_t i = 0; i < mesh->mNumBones; ++i) {
            struct aiBone *bone = mesh->mBones[i];
            
            //SDL_Log("bone: %s - armature node: %s - scene node: %s", bone->mName.data, bone->mArmature->mName.data, bone->mNode->mName.data);

            node_map[i] = bone->mNode;
            tmp_heirarchy[i] = bone->mNode->mParent;
            convert_to_cglm(local_pose[i], &bone->mNode->mTransformation);
            //dump_mat("bone local pose", local_pose[i]);

            convert_to_cglm(inverse_bind[i], &bone->mOffsetMatrix);
            //dump_mat("bone inverse bind", inverse_bind[i]);

            struct import_mapping mapping = {
                .name = &bone->mNode->mName,
                .node = bone->mNode,
                .skm = output,
                .skm_bone_idx = i,
            };
            // todo proper stuff...
            assert(mapping_count < 1024);
            mappings[mapping_count++] = mapping;

            for(size_t j = 0; j < bone->mNumWeights; ++j) {
                size_t vert = bone->mWeights[j].mVertexId;
                float weight = bone->mWeights[j].mWeight;

                size_t i3 = vert * 14;
                for(size_t check = 10; check < 14; ++check) {
                    if(vert_data[i3 + check] < 0) {
                        // We can use this slot

                        size_t weight_idx = i3 + check - 4;
                        size_t weight_idx_idx = i3 + check;

                        vert_data[weight_idx] = weight;
                        vert_data[weight_idx_idx] = (float)(i);

                        //SDL_Log("weights: [%zu] = %f [%zu] = %f", weight_idx, weight, weight_idx_idx, (float)(i));
                        
                        break;
                    }
                }               
            }
        }

        for(size_t i = 0; i < mesh->mNumBones; ++i) {
            // Find parent index.
            struct aiNode *parent = tmp_heirarchy[i];
            if(!parent) {
                heirarchy[i] = -1;
                continue;
            }

            // Initialize the value to -1, in case the parent is non-NULL but
            // is another node in the tree (which it probably is.)
            //
            // We could also check against the mArmature probably.
            heirarchy[i] = -1;
            for(int parent_idx = 0; parent_idx < mesh->mNumBones; ++parent_idx) {
                if(parent == node_map[parent_idx]) {
                    heirarchy[i] = parent_idx;
                    break;
                }
            }
        }

        eng_free(tmp_heirarchy, sizeof(*tmp_heirarchy) * mesh->mNumBones);
        eng_free(node_map, sizeof(*node_map) * mesh->mNumBones);
    }

    // TODO: We should just have a shader (?) that we provide here (?)
    skm_init(output, vert_data, vert_arrsize, elem_data, elem_arrsize, output->shader);
    output->bone_inverse_bind = inverse_bind;
    output->bone_pose = pose;
    output->bone_local_pose = local_pose;
    output->bone_heirarchy = heirarchy;
    output->bone_count = mesh->mNumBones;

    output->bone_compute_graph = eng_zalloc(sizeof(*output->bone_compute_graph) * mesh->mNumBones);

    output->import_key = mesh;
}

void
handle_skeleton(struct aiSkeleton *skeleton, struct import_data *id) {
    if(!skeleton->mBones || skeleton->mNumBones < 1) return;

    struct skeletal_mesh *output = NULL;

    for(size_t i = 0; i < id->got_skm; ++i) {
        if(skeleton->mBones[0]->mMeshId == id->skm[i]->import_key) {
            output = id->skm[i];
            SDL_Log("skeleton import: got matching skeletal_mesh");
            goto import;
        }
    }

    SDL_Log("skeleton import: no matching skeletal_mesh");
    return;

import:
    for(size_t i = 0; i < skeleton->mNumBones; ++i) {
        if(i >= output->bone_count) return;

        struct aiSkeletonBone *bone = skeleton->mBones[i];

        convert_to_cglm(output->bone_local_pose[i], &bone->mLocalMatrix);
        output->bone_heirarchy[i] = bone->mParent;
    }
}

static bool
ai_str_eq(struct aiString *a, struct aiString *b) {
    if(a == b) return true;
    if(a->length != b->length) return false;

    return !strcmp(a->data, b->data);
}

struct import_mapping*
identify(struct aiString *node_name) {
    for(size_t j = 0; j < mapping_count; ++j) {
        if(ai_str_eq(node_name, mappings[j].name)) {
            return &mappings[j];
        }
    }
    return NULL;
}

void
handle_animation(struct aiAnimation *animation, const struct aiScene *scene, struct import_data *id) {
    SDL_Log("Considering importing animation %s", animation->mName.data);
    if(animation->mNumChannels < 1) return; // Don't process if we can't ID skeleton node

    struct skeletal_mesh *skm = NULL;
    for(size_t i = 0; i < animation->mNumChannels; ++i) {
        struct aiString *name = &animation->mChannels[i]->mNodeName;
        struct import_mapping *map = identify(name);
        if(map) { skm = map->skm; break; }
    }

    if(!skm) return; // Don't process if we can't ID skeleton node

    struct skm_armature_anim *output = NULL;
    if(id->got_skm_arm_anim < id->num_skm_arm_anim) {
        output = id->skm_arm_anim[id->got_skm_arm_anim++];
    }
    else return;

    SDL_Log("Importing animation -- skm = %p, output = %p", skm, output);
    output->skm = skm;

    // For now, just process the node animations. First, allocate enough
    // data for each bone in the skeletal mesh.
    output->bones = eng_zalloc(sizeof(*output->bones) * skm->bone_count);
    output->length = animation->mDuration / animation->mTicksPerSecond;
    
    // TODO: Maybe initialize the bones data to NULL? Should not be necessary though.
    for(size_t i = 0; i < animation->mNumChannels; ++i) {
        struct aiNodeAnim *anim = animation->mChannels[i];

        struct import_mapping *map = identify(&anim->mNodeName);
        if(!map) continue; // Skip unknown bones

        #define ALLOC(dest, dest_count, src_count) \
        if(src_count > 0) { \
            dest = eng_zalloc(sizeof(*dest) * src_count); \
            dest_count = src_count; \
        } else { dest_count = 1; dest = eng_zalloc(sizeof(*dest)); } // TODO identity..?

        struct skm_arm_anim_bone *bone = &output->bones[map->skm_bone_idx];
        ALLOC(bone->position, bone->position_count, anim->mNumPositionKeys);
        ALLOC(bone->scale, bone->scale_count, anim->mNumScalingKeys);
        ALLOC(bone->rotation, bone->rotation_count, anim->mNumRotationKeys);

        for(size_t j = 0; j < anim->mNumPositionKeys; ++j) {
            bone->position[j].time = (float)anim->mPositionKeys[j].mTime / animation->mTicksPerSecond;
            cvt_vec3(bone->position[j].value, &anim->mPositionKeys[j].mValue);
        }

        for(size_t j = 0; j < anim->mNumScalingKeys; ++j) {
            bone->scale[j].time = (float)anim->mScalingKeys[j].mTime / animation->mTicksPerSecond;
            cvt_vec3(bone->scale[j].value, &anim->mScalingKeys[j].mValue);
        }

        for(size_t j = 0; j < anim->mNumRotationKeys; ++j) {
            bone->rotation[j].time = (float)anim->mRotationKeys[j].mTime / animation->mTicksPerSecond;
            cvt_quat(bone->rotation[j].value, &anim->mRotationKeys[j].mValue);
        }
    }
}

// Walks over all the nodes in a scene. Not needed right now -- we don't care
// about the heirarchy yet, only the mesh data.
void
handle_node(struct aiNode *node, const struct aiScene *scene, struct import_data *id, int idx) {
   // for(size_t i = 0; i < node->mNumMeshes; ++i) {
    //    handle_mesh(scene->mMeshes[node->mMeshes[i]], id);
    //}

    SDL_Log("%d - traverse: %s", idx, node->mName.data);

    for(size_t i = 0; i < node->mNumChildren; ++i) {
        handle_node(node->mChildren[i], scene, id, idx + 1);
    }
}

void
load_model(const char *path, struct import_data *id) {
    const struct aiScene *scene = aiImportFile(path, aiProcess_Triangulate | aiProcess_PopulateArmatureData);
    mapping_count = 0;
    if(!scene) {
        SDL_Log("failed to import scene: %s\n", aiGetErrorString());
        return;
    }

    SDL_Log("model: got %d meshes, %d skeletons\n", scene->mNumMeshes, scene->mNumSkeletons);

    for(size_t i = 0; i < scene->mNumMeshes; ++i) {
        handle_mesh(scene->mMeshes[i], id);
    }

    for(size_t i = 0; i < scene->mNumAnimations; ++i) {
        handle_animation(scene->mAnimations[i], scene, id);
    }

    // for(size_t i = 0; i < scene->mNumSkeletons; ++i) {
    //     handle_skeleton(scene->mSkeletons[i], id);
    // }

   // handle_node(scene->mRootNode, scene, id, 0);

    SDL_Log("model: imported %s\n", path);

    aiReleaseImport(scene);
}