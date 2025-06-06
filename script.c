#include "engine/skeletal_mesh.h"
#include "engine/shader.h"
#include "engine/our_gl.h"
#include "engine/model.h"
#include "engine/alloc.h"

#include "engine/serialize/serialize_skm.h"

#include "obj/shader.h"

#include "map.h"
#include "actions.h"

#include <cglm/cglm.h>

#include "nuklear-cfg.h"

#include <SDL3_mixer/SDL_mixer.h>

struct skeletal_mesh player_mesh = {0};
struct skm_armature_anim player_walk_anim = {0};
struct skm_armature_anim_playback player_walk_playback = {0};

struct skm_armature_anim player_idle_anim = {0};
struct skm_armature_anim_playback player_idle_playback = {0};

struct skm_armature_anim player_jump_anim = {0};
struct skm_armature_anim_playback player_jump_playback = {0};

struct skm_armature_anim player_jump_down_anim = {0};
struct skm_armature_anim_playback player_jump_down_playback = {0};

struct skeletal_mesh hay_mesh = {0};

struct skeletal_mesh carrot_mesh = {0};

Mix_Chunk *sound_chomp;
Mix_Chunk *sound_boing;

struct carrot {
    vec2 position;

    float rotation;
    float scale;

    bool eaten;
};

size_t carrot_count = 0;
size_t got_carrot_count = 0;

bool jump_unlocked = false;
float jump_message_timer = 0.0;
float win_message_timer = 0.0;

struct carrot carrots[256] = {0}; 
// {
//     {
//         .position = { 2.0 * 1, 2.0 * 2 },
//         .rotation = 0
//     }
// };

int
sign_of(float f) {
    if (f < 0) return -1;
    if (f > 0) return 1;
    return 0;
}

GLuint
generate_null_texture() {
    GLuint tex;
    REPORT(glGenTextures(1, &tex));

    uint8_t data[] = { 255, 255, 255, 255 };

    REPORT(glBindTexture(GL_TEXTURE_2D, tex));

    REPORT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    REPORT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    REPORT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    REPORT(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    REPORT(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));
    REPORT(glGenerateMipmap(GL_TEXTURE_2D));

    return tex;
}

GLuint null_texture = 0;

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

    GLuint albedo;

    GLuint self; // the shader program
} skel_pbr;

GLuint player_tex = 0;
GLuint hay_tex = 0;
GLuint carrot_tex = 0;

// projection matrix
static mat4 p_matrix;

// view matrix
static mat4 v_matrix;

struct {
    struct phys_obj obj;
    
    vec2 velocity;

    mat4 model_matrix;
} player;

struct {
    GLuint v;
    GLuint p;
    GLuint m;

    GLuint base_color;
    GLuint metallic;
    GLuint perceptual_roughness;

    GLuint albedo;

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

// used to make the hay look slightly more interesting.
float
nudge() {
    // PROBLEM: Our mesh import apparently splits the vertices.
    // If we can fix this, then we can do this.

    // this is very bad, but it's ok.
    float x = (float)rand() / (float)RAND_MAX;

    // up to 5 thousandths in any direction.
    return (x - 0.5) / 1000.0;
}

float
rand_angle() {
    int x = rand() % 4;
    return x * 6.28 * 0.25;
}

#define LEVEL_MESH_ATTRIBS 8

void
copy_hay_mesh(float *verts, GLuint *tris, GLuint *vertptr, GLuint *triptr, size_t vert_data_count,
        size_t tri_data_count, int x, int y) {
    
    // The actual vertex indices into the array, as far as the GPU is concerned,
    // are real vertex indices, i.e. the sub_data pointer divided by 6.
    GLuint tri_base = *vertptr / LEVEL_MESH_ATTRIBS;

    float off_x = x * 2 + nudge();
    float off_y = y * 2 + nudge();
    float off_z = nudge();

    // Unless we nudge all the vertices that are at thes ame location (but that
    // have different normals?) by the same amount, we can't do the nugding. but
    // we can nudge entire cubes.

    mat4 cube_tform;
    glm_rotate_make(cube_tform, rand_angle(), (vec3){ 0, 0, 1 });
    glm_rotated(cube_tform, rand_angle() + nudge() * 250, (vec3){ 0, 1, 0 });
    glm_translated(cube_tform, (vec3){ off_x, off_y, off_z });

    mat4 normal_mat;
    glm_mat4_copy(cube_tform, normal_mat);
    glm_mat4_transpose(normal_mat);
    glm_mat4_inv(normal_mat, normal_mat);

    for(size_t i = 0; i < vert_data_count; ++i) {
        size_t i6 = *vertptr;
        size_t i14 = i * SKEL_MESH_4BYTES_COUNT;

        vec4 pos = {
            hay_mesh.vertices[i14 + 0],
            hay_mesh.vertices[i14 + 1],
            hay_mesh.vertices[i14 + 2],
            1.0
        };

        vec4 norm = {
            hay_mesh.vertices[i14 + 3],
            hay_mesh.vertices[i14 + 4],
            hay_mesh.vertices[i14 + 5],
            0.0
        };

        glm_mat4_mulv(cube_tform, pos, pos);
        glm_mat4_mulv(normal_mat, norm, norm);

        verts[i6 + 0] = pos[0];
        verts[i6 + 1] = pos[1];
        verts[i6 + 2] = pos[2];
        verts[i6 + 3] = norm[0];
        verts[i6 + 4] = norm[1];
        verts[i6 + 5] = norm[2];

        verts[i6 + 6] = hay_mesh.vertices[i14 + 14];
        verts[i6 + 7] = hay_mesh.vertices[i14 + 15];

        *vertptr += LEVEL_MESH_ATTRIBS;
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

            if(map_get(map, x, y) == CELL_CARROT) {
                // Instantiate a carrot.
                carrots[carrot_count++] = (struct carrot){
                    .position = { x * 2, y * 2 },
                    .rotation = 0,
                    .scale = 1,
                    .eaten = false
                };

                // Clear the carrot for physics engine
                map_set(map, x, y, CELL_EMPTY);
            }
        }
    }

    size_t vert_data_count = hay_mesh.vertices_count / SKEL_MESH_4BYTES_COUNT;
    size_t tri_data_count = hay_mesh.triangles_count / 3;

    // Fow now, just clone the vertex data for every vertex. We could try to 
    // find a way to only have one copy of normals.
    size_t verts_size = sizeof(float) * LEVEL_MESH_ATTRIBS * hay_count * vert_data_count;
    float *verts = eng_zalloc(verts_size);
    size_t tris_size = sizeof(GLuint) * 3 * hay_count * tri_data_count;
    GLuint *tris = eng_zalloc(tris_size);

    GLuint vertptr = 0;
    GLuint triptr = 0;

    level_mesh.triangle_count = hay_count * tri_data_count * 3;

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

void init_player();

static Mix_Music *game_music = NULL;

const float anim_start_seek = 60.0 / 24.0;

void
init() {
    static_pbr.self = ourgl_compile_shader(static_vert_src, skel_frag_src);
    REPORT(static_pbr.p = glGetUniformLocation(static_pbr.self, "u_p"));
    REPORT(static_pbr.v = glGetUniformLocation(static_pbr.self, "u_v"));
    REPORT(static_pbr.m = glGetUniformLocation(static_pbr.self, "u_m"));
    REPORT(static_pbr.metallic = glGetUniformLocation(static_pbr.self, "metallic"));
    REPORT(static_pbr.perceptual_roughness = glGetUniformLocation(static_pbr.self, "perceptual_roughness"));
    REPORT(static_pbr.base_color = glGetUniformLocation(static_pbr.self, "base_color"));
    REPORT(static_pbr.albedo = glGetUniformLocation(static_pbr.self, "u_albedo"));

    skel_pbr.self = ourgl_compile_shader(skel_vert_src, skel_frag_src);

    REPORT(skel_pbr.p = glGetUniformLocation(skel_pbr.self, "u_p"));
    REPORT(skel_pbr.v = glGetUniformLocation(skel_pbr.self, "u_v"));

    REPORT(skel_pbr.skeleton = glGetUniformLocation(skel_pbr.self, "u_skeleton"));
    REPORT(skel_pbr.skeleton_count = glGetUniformLocation(skel_pbr.self, "u_skeleton_count"));

    REPORT(skel_pbr.metallic = glGetUniformLocation(skel_pbr.self, "metallic"));
    REPORT(skel_pbr.perceptual_roughness = glGetUniformLocation(skel_pbr.self, "perceptual_roughness"));
    REPORT(skel_pbr.base_color = glGetUniformLocation(skel_pbr.self, "base_color"));
    REPORT(skel_pbr.albedo = glGetUniformLocation(skel_pbr.self, "u_albedo"));

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

        .skm_arm_anim = (struct skm_armature_anim*[]){ &player_idle_anim, &player_jump_anim, &player_jump_down_anim, &player_walk_anim },
        .num_skm_arm_anim = 4,
        .got_skm_arm_anim = 0,

        .texture = (GLuint[]){ 0 },
        .num_texture = 1,
        .got_texture = 0,
    };

    struct import_data hay_id = {
        .skm = (struct skeletal_mesh*[]){ &hay_mesh },
        .num_skm = 1,
        .got_skm = 0,

        .skm_arm_anim = NULL,
        .num_skm_arm_anim = 0,
        .got_skm_arm_anim = 0,

        .texture = (GLuint[]) { 0 },
        .num_texture = 1,
        .got_texture = 0,
    };

    struct import_data carrot_id = {
        .skm = (struct skeletal_mesh*[]){ &carrot_mesh },
        .num_skm = 1,
        .got_skm = 0,

        .skm_arm_anim = NULL,
        .num_skm_arm_anim = 0,
        .got_skm_arm_anim = 0,

        .texture = (GLuint[]) { 0 },
        .num_texture = 1,
        .got_texture = 0,
    };

    load_model("blender/horse.glb", &player_id);
    load_model("blender/hay.glb", &hay_id);
    load_model("blender/carrot.glb", &carrot_id);

    player_tex = player_id.texture[0];
    hay_tex = hay_id.texture[0];
    carrot_tex = carrot_id.texture[0];

    game_music = Mix_LoadMUS("sounds/music.ogg");
    SDL_Log("music error: %s", SDL_GetError());
    //assert(game_music);
    if(game_music != NULL) {
        Mix_PlayMusic(game_music, -1);
    }

    sound_chomp = Mix_LoadWAV("sounds/chomp.wav");
    sound_boing = Mix_LoadWAV("sounds/boing.wav");

    skm_arm_playback_init(&player_walk_playback, &player_walk_anim);
    skm_arm_playback_init(&player_idle_playback, &player_idle_anim);
    skm_arm_playback_init(&player_jump_playback, &player_jump_anim);
    skm_arm_playback_init(&player_jump_down_playback, &player_jump_down_anim);

    player_mesh.shader = skel_pbr.self;
    skm_gl_init(&player_mesh);

    skm_gl_init(&carrot_mesh);

    REPORT(glUseProgram(skel_pbr.self));
    REPORT(glUniform1f(skel_pbr.skeleton_count, (float)player_mesh.bone_count));
    REPORT(glUniform1i(skel_pbr.skeleton, 1)); // match GL_TEXTURE1 from skeletal_mesh.c

    SDL_Log("init called.");

    init_player();

    init_level_gl();
    gen_level_mesh(&map0);

    null_texture = generate_null_texture();

    // Initialize looping animations to the loop point.
    skm_arm_playback_seek(&player_walk_playback, anim_start_seek);
    skm_arm_playback_seek(&player_idle_playback, anim_start_seek);
    skm_arm_playback_seek(&player_jump_playback, anim_start_seek);
    skm_arm_playback_seek(&player_jump_down_playback, anim_start_seek);
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
init_player() {
    player.obj.on_floor = false;
    glm_vec2_copy((vec2){ 10 * 2, 5 * 2 }, player.obj.pos);
    glm_vec2_copy((vec2){ -0.9, -1 }, player.obj.top_left);
    // Ok, this really should probably be top_left = -0.8, but this gives us
    // what we want for now, so I guess just let it ride...
    glm_vec2_copy((vec2){ 0.9, 0.8 }, player.obj.bottom_right);

    player.obj.col_normal_count = 0;

    glm_vec2_copy((vec2){ 0, -10 }, player.velocity);
}

struct action act_left = {
    .code = SDLK_A
};
struct action act_right = {
    .code = SDLK_D
};
struct action act_jump = {
    .code = SDLK_SPACE
};

float max_walk_vel = 0.8f;
float max_jump_vel = 0.9f;

void
physics_player(double dt) {
    vec2 gravity = { 0, -36 * dt };
    glm_vec2_add(player.velocity, gravity, player.velocity);

    float h_vel = 0.0;
    if(act_left.is_pressed) {
        h_vel = -1;
    }
    else if(act_right.is_pressed) {
        h_vel = 1;
    }

    float max_vel = max_walk_vel;
    if(!player.obj.on_floor) {
        // move fast in midair
        max_vel = max_jump_vel;
    }

    float target_vel = max_vel * h_vel;
    float dif_from_target = target_vel - player.velocity[0];
    float accel = 24.0f * sign_of(dif_from_target);
    float accel_integrated = accel * dt;
    if(fabs(accel_integrated) > fabs(dif_from_target)) {
        accel_integrated = dif_from_target;
    }

    player.velocity[0] += accel_integrated;

    if(act_just_pressed(&act_jump) && player.obj.on_floor && jump_unlocked) {
        // jump impulse
        player.velocity[1] = 19.0f;
        Mix_PlayChannel(-1, sound_boing, 0);
    }

    const float margin = 1.0 / 65536.0;
    phys_slide_motion_solver(player.velocity, player.velocity, &player.obj, margin, dt);
}

float anim_transition_blend = 1.0;
struct skm_armature_anim_playback *anim_prev = &player_idle_playback;
struct skm_armature_anim_playback *anim_cur  = &player_idle_playback;

float player_rot_y =  3.14159265 / 2.0;
float player_rot_y_target =  3.14159265 / 2.0;

void
step(float *x, float target, float speed) {
    float dist = target - *x;
    if(fabs(speed) > fabs(dist)) {
        speed = dist;
    }
    else if(sign_of(speed) == -sign_of(dist)) {
        speed *= -1;
    }
    *x += speed;
}

void
animate_player(double dt) {
    struct skm_armature_anim_playback *next = &player_idle_playback;

    if(fabs(player.velocity[0]) > 0.2) {
        next = &player_walk_playback;
    }
    if(fabs(player.velocity[1]) > 0.1 || !player.obj.on_floor) {
        next = &player_jump_playback;
        if(player.velocity[1] < 0) {
            next = &player_jump_down_playback;
        }
    }
    //next = &player_idle_playback;

    if(player.velocity[0] > 0.2) {
        player_rot_y_target = 3.14159265 / 2.0;
    }
    if(player.velocity[0] < -0.2) {
        player_rot_y_target = -3.14159265 / 2.0;
    }
    step(&player_rot_y, player_rot_y_target, 3.14 * dt * 3.0);

    step(&anim_transition_blend, 1.0, 6.0 * dt);
    if(next != anim_cur) {
        //if(anim_transition_blend >= 0.5) {
        //    
        //}
        

        // Only ever transition "fully" from one animation to another.
        // Otherwise, keep the existing previous animation and blend from
        // that one to the new one.
        if(anim_transition_blend >= 0.99) {
            anim_transition_blend = 0.0;
            anim_prev = anim_cur;
        }

        anim_cur = next;
    }
}

void
tick_player(double dt) {
    physics_player(dt);
    animate_player(dt);

    // Reset the player's matrix.
    glm_mat4_identity(player.model_matrix);
    glm_rotated(player.model_matrix, player_rot_y, (vec3){ 0, 1, 0 });
    glm_translated(player.model_matrix, (vec3){ player.obj.pos[0], player.obj.pos[1], 0.0 });

    // Set up camera.
    glm_mat4_identity(v_matrix);

    glm_translated(v_matrix, (vec3){ 0, 0, DIST_FROM_CAM });

    mat4 help;
    // normal version:
    glm_rotate_make(help, -0.3, (vec3) { 1.0, 0.0, 0.0 });
    // thumbnail edition:
    //glm_rotate_make(help, 3.14/4, (vec3){ 0.0, 1.0, 0.0 });
    glm_mul(help, v_matrix, v_matrix);


    mat4 help2;
    glm_translate_make(help2, (vec3){ player.obj.pos[0], player.obj.pos[1], 0 });
    glm_mul(help2, v_matrix, v_matrix);
    
    glm_inv_tr(v_matrix);

    pass_vp();
}

// void
// get_single_anim_bone(mat4 out, struct skm_armature_anim_playback *playback, size_t i) {
//     glm_mat4_copy(playback->state[i].scale_matrix, out);
//     glm_mat4_mul(playback->state[i].rotation_matrix, out, out);
//     glm_mat4_mul(playback->state[i].position_matrix, out, out);
// }

CGLM_INLINE
void
my_quat_mat4(versor q, mat4 dest) {
  double w, x, y, z,
        xx, yy, zz,
        xy, yz, xz,
        wx, wy, wz, norm, s;

  norm = glm_quat_norm(q);
  //s    = norm > 0.0f ? 2.0f / norm : 0.0f;
  norm = (norm > 0.0 ? norm : INFINITY);

  x = q[0];
  y = q[1];
  z = q[2];
  w = q[3];

  xx = 2.0 * x * x / norm;   xy = 2.0 * x * y / norm;   wx = 2.0 * w * x / norm;
  yy = 2.0 * y * y / norm;   yz = 2.0 * y * z / norm;   wy = 2.0 * w * y / norm;
  zz = 2.0 * z * z / norm;   xz = 2.0 * x * z / norm;   wz = 2.0 * w * z / norm;

  dest[0][0] = 1.0f - yy - zz;
  dest[1][1] = 1.0f - xx - zz;
  dest[2][2] = 1.0f - xx - yy;

  dest[0][1] = xy + wz;
  dest[1][2] = yz + wx;
  dest[2][0] = xz + wy;

  dest[1][0] = xy - wz;
  dest[2][1] = yz - wx;
  dest[0][2] = xz - wy;

  dest[0][3] = 0.0f;
  dest[1][3] = 0.0f;
  dest[2][3] = 0.0f;
  dest[3][0] = 0.0f;
  dest[3][1] = 0.0f;
  dest[3][2] = 0.0f;
  dest[3][3] = 1.0f;
}

// void
// my_quat_mat4(versor rot, mat4 matrix) {
//     float norm = glm_quat_norm(rot);
//     if(norm < 0.0001) {
//         glm_mat4_identity(matrix);
//         return;
//     }
//     glm_quat_mat4(rot, matrix);
// }

void
apply_playbacks() {
    struct skeletal_mesh *skm = &player_mesh;

    struct skm_armature_anim_playback *anim1 = anim_prev;
    struct skm_armature_anim_playback *anim2 = anim_cur;
    float blend = anim_transition_blend;

    for(size_t i = 0; i < skm->bone_count; ++i) {
        mat4 translate_matrix;
        mat4 rotate_matrix;
        mat4 scale_matrix;

        vec3 pos;
        versor rot;
        vec3 scale = GLM_VEC3_ONE_INIT;

        glm_vec3_lerp(anim1->state[i].position, anim2->state[i].position, blend, pos);
        //glm_vec3_lerp(anim1->state[i].scale, anim2->state[i].scale, blend, scale);
        glm_quat_normalize(anim1->state[i].rotation);
        glm_quat_normalize(anim2->state[i].rotation);
        glm_quat_slerp(anim1->state[i].rotation, anim2->state[i].rotation, blend, rot);

        // apparently this particular normalize() is needed for things not to blow
        // up. Why? who the hell knows.
        //
        // oh. norm is very different from normalize for quaternions.
        glm_quat_normalize(rot);

        // Hmm. We still have occasional jank. From the one frame I manged to
        // capture, it didn't look like there was any scaling, but that the
        // player model was facing entirely the wrong way. Weird..

        glm_translate_make(translate_matrix, pos);
        my_quat_mat4(rot, rotate_matrix);
        glm_scale_make(scale_matrix, scale);

        if(fabs(glm_mat4_det(rotate_matrix)) < 0.8) {
            glm_mat4_identity(rotate_matrix);
            SDL_Log("bone = %llu", i);
            SDL_Log("rot0 = %f %f %f %f", anim1->state[i].rotation[0], anim1->state[i].rotation[1], anim1->state[i].rotation[2], anim1->state[i].rotation[3]);
            SDL_Log("rot1 = %f %f %f %f", anim2->state[i].rotation[0], anim2->state[i].rotation[1], anim2->state[i].rotation[2], anim2->state[i].rotation[3]);
            SDL_Log("blend = %f %f %f %f", rot[0], rot[1], rot[2], rot[3]);
        }

        glm_mat4_copy(scale_matrix, skm->bone_local_pose[i]);
        glm_mat4_mul(rotate_matrix, skm->bone_local_pose[i], skm->bone_local_pose[i]);
        glm_mat4_mul(translate_matrix, skm->bone_local_pose[i], skm->bone_local_pose[i]);
    }
}

void
eat_carrot() {
    // TODO: Increase player max speed?
    Mix_PlayChannel(-1, sound_chomp, 0);

    max_walk_vel += 0.2f;
    max_jump_vel += 0.3f;

    got_carrot_count += 1;

    if(got_carrot_count == 5) {
        // Unlock jump.
        jump_unlocked = true;
        jump_message_timer = 2.0;
    }
    if(got_carrot_count == carrot_count) {
        win_message_timer = 4.0;
    }
}

void
tick_carrots(double dt) {
    for(size_t i = 0; i < carrot_count; ++i) {
        carrots[i].rotation += dt * 3.0;
        if(carrots[i].rotation > 6.28) {
            carrots[i].rotation -= 6.28;
        }

        if(carrots[i].eaten) {
            // If we were eaten, scale down and go towards the player.
            if(carrots[i].scale > 0.0) {
                step(&carrots[i].scale, 0.0, dt * 2.0);

                vec2 dif;
                glm_vec2_sub(player.obj.pos, carrots[i].position, dif);
                glm_vec2_scale(dif, 0.2 * dt, dif);
                glm_vec2_add(dif, carrots[i].position, carrots[i].position);
            }
        }
        else {
            // Otherwise, check if we're near the player and set eaten if
            // so.
            vec2 dif;
            glm_vec2_sub(player.obj.pos, carrots[i].position, dif);
            if(glm_vec2_norm2(dif) < 0.8 * 0.8) {
                //SDL_Log("eat da carrot");
                carrots[i].eaten = true;
                eat_carrot();
            }
        }
    }
}

void
tick(double dt) {
    tick_player(dt);
    tick_carrots(dt);

    //skm_arm_playback_apply(&player_walk_playback);
    //skm_arm_playback_apply(&player_idle_playback);
    //skm_arm_playback_apply(&player_jump_playback);
    apply_playbacks();

    if(jump_message_timer > 0.0) { jump_message_timer -= dt; }
    
    // compute previous frame?
    skm_compute_matrices(&player_mesh, player.model_matrix);

    const float anim_loop_length = 60.0 / 24.0;
    
    const float anim_boundary = anim_start_seek + anim_loop_length;


    const double anim_ref_vel = 1.157943 / anim_loop_length; 
    double anim_step = 1.0 * dt;
    if(anim_cur == &player_walk_playback) {
        anim_step = (fabs(player.velocity[0]) / anim_ref_vel) * dt;
    }

    

    skm_arm_playback_step(&player_walk_playback, anim_step);
    if(player_walk_playback.time >= anim_boundary) {
        skm_arm_playback_seek(&player_walk_playback, player_walk_playback.time - anim_loop_length);
    }
    skm_arm_playback_step(&player_idle_playback, anim_step);
    if(player_idle_playback.time >= anim_boundary) {
        skm_arm_playback_seek(&player_idle_playback, player_idle_playback.time - anim_loop_length);
    }
    skm_arm_playback_step(&player_jump_playback, anim_step);
    if(player_jump_playback.time >= anim_boundary) {
        skm_arm_playback_seek(&player_jump_playback, player_jump_playback.time - anim_loop_length);
    }
    skm_arm_playback_step(&player_jump_down_playback, anim_step);
    if(player_jump_down_playback.time >= anim_boundary) {
        skm_arm_playback_seek(&player_jump_down_playback, player_jump_down_playback.time - anim_loop_length);
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
render_carrots() {
    for(size_t i = 0; i < carrot_count; ++i) {
        mat4 tform;

        glm_scale_make(tform, (vec3){ carrots[i].scale, carrots[i].scale, carrots[i].scale });
        glm_rotated(tform, carrots[i].rotation, (vec3){ 0, 1, 0 });
        glm_translated(tform, (vec3){ carrots[i].position[0], carrots[i].position[1], 0.0 });

        REPORT(glUniformMatrix4fv(static_pbr.m, 1, false, tform[0]));
        REPORT(glDrawElements(GL_TRIANGLES, carrot_mesh.triangles_count, GL_UNSIGNED_INT, 0));

    }
}

void
render() {
    REPORT(glUseProgram(skel_pbr.self));
    glUniform1f(skel_pbr.metallic, 0.0);
    glUniform1f(skel_pbr.perceptual_roughness, 0.3);
    glUniform3f(skel_pbr.base_color, 1.0, 1.0, 1.0); // multiplied by texture

    REPORT(glActiveTexture(GL_TEXTURE0));
    REPORT(glBindTexture(GL_TEXTURE_2D, player_tex));

    REPORT(glUniform1i(skel_pbr.albedo, 0));

    skm_gl_draw(&player_mesh);

    REPORT(glUseProgram(static_pbr.self));
    glUniform1f(static_pbr.metallic, 0.0);
    glUniform1f(static_pbr.perceptual_roughness, 0.7);
    //glUniform3f(static_pbr.base_color, 246.0/255.0, 247.0/255.0, 146.0/255.0);
    glUniform3f(static_pbr.base_color, 1.0, 1.0, 1.0);

    REPORT(glActiveTexture(GL_TEXTURE0));
    REPORT(glBindTexture(GL_TEXTURE_2D, carrot_tex));

    REPORT(glUniform1i(static_pbr.albedo, 0));

    REPORT(glBindBuffer(GL_ARRAY_BUFFER, carrot_mesh.array_buf));
    REPORT(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, carrot_mesh.element_buf));

    // Apparently we have to glVertexAttribPointer even though we already did inside skm_gl_draw...?
    REPORT(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, SKEL_MESH_4BYTES_COUNT * sizeof(float), (void*)0));
    REPORT(glEnableVertexAttribArray(0));

    REPORT(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, SKEL_MESH_4BYTES_COUNT * sizeof(float), (void*)(sizeof(float) * 3)));
    REPORT(glEnableVertexAttribArray(1));

    REPORT(glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, SKEL_MESH_4BYTES_COUNT * sizeof(float), (void*)(sizeof(float) * 6)));
    REPORT(glEnableVertexAttribArray(2));

    REPORT(glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, SKEL_MESH_4BYTES_COUNT * sizeof(float), (void*)(sizeof(float) * 10)));
    REPORT(glEnableVertexAttribArray(3));

    REPORT(glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, SKEL_MESH_4BYTES_COUNT * sizeof(float), (void*)(sizeof(float) * 14)));
    REPORT(glEnableVertexAttribArray(4));

    render_carrots();





    REPORT(glBindBuffer(GL_ARRAY_BUFFER, level_mesh.array_buf));
    REPORT(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, level_mesh.element_buf));

    REPORT(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, LEVEL_MESH_ATTRIBS * sizeof(float), (void*)0));
    REPORT(glEnableVertexAttribArray(0));

    REPORT(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, LEVEL_MESH_ATTRIBS * sizeof(float), (void*)(sizeof(float) * 3)));
    REPORT(glEnableVertexAttribArray(1));

    REPORT(glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, LEVEL_MESH_ATTRIBS * sizeof(float), (void*)(sizeof(float) * 6)));
    REPORT(glEnableVertexAttribArray(4));

    //SDL_Log("level mesh tri count: %u\n", level_mesh.triangle_count);

    REPORT(glUseProgram(static_pbr.self));
    glUniform1f(static_pbr.metallic, 0.0);
    glUniform1f(static_pbr.perceptual_roughness, 0.95);
    //glUniform3f(static_pbr.base_color, 246.0/255.0, 247.0/255.0, 146.0/255.0);
    glUniform3f(static_pbr.base_color, 1.0, 1.0, 1.0);

    // hay bales are drawn with an identity matrix.
    mat4 identity = GLM_MAT4_IDENTITY_INIT;
    REPORT(glUniformMatrix4fv(static_pbr.m, 1, false, identity[0]));

    REPORT(glActiveTexture(GL_TEXTURE0));
    REPORT(glBindTexture(GL_TEXTURE_2D, hay_tex));

    REPORT(glUniform1i(static_pbr.albedo, 0));

    REPORT(glDrawElements(GL_TRIANGLES, level_mesh.triangle_count, GL_UNSIGNED_INT, 0));
}

void
ui_theme(struct nk_context *ctx) {
    struct nk_color table[NK_COLOR_COUNT];
    table[NK_COLOR_TEXT] = nk_rgba(0, 0, 0, 255);
	table[NK_COLOR_WINDOW] = nk_rgba(0, 0, 0, 0);
	table[NK_COLOR_HEADER] = nk_rgba(0, 0, 0, 0);
	table[NK_COLOR_BORDER] = nk_rgba(0, 0, 0, 0);
	table[NK_COLOR_BUTTON] = nk_rgba(230, 230, 230, 255);
	table[NK_COLOR_BUTTON_HOVER] = nk_rgba(240, 240, 240, 255);
	table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(210, 210, 210, 255);
	table[NK_COLOR_TOGGLE] = nk_rgba(50, 58, 61, 255);
	table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 53, 56, 255);
	table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(48, 83, 111, 255);
	table[NK_COLOR_SELECT] = nk_rgba(57, 67, 61, 255);
	table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(48, 83, 111, 255);
	table[NK_COLOR_SLIDER] = nk_rgba(50, 58, 61, 255);
	table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(48, 83, 111, 245);
	table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
	table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
	table[NK_COLOR_PROPERTY] = nk_rgba(50, 58, 61, 255);
	table[NK_COLOR_EDIT] = nk_rgba(50, 58, 61, 225);
	table[NK_COLOR_EDIT_CURSOR] = nk_rgba(210, 210, 210, 255);
	table[NK_COLOR_COMBO] = nk_rgba(50, 58, 61, 255);
	table[NK_COLOR_CHART] = nk_rgba(50, 58, 61, 255);
	table[NK_COLOR_CHART_COLOR] = nk_rgba(48, 83, 111, 255);
	table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
	table[NK_COLOR_SCROLLBAR] = nk_rgba(50, 58, 61, 255);
	table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(48, 83, 111, 255);
	table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
	table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
	table[NK_COLOR_TAB_HEADER] = nk_rgba(48, 83, 111, 255);
    nk_style_from_table(ctx, table);
}

void
ui(struct nk_context *ctx, int win_width, int win_height) {
    float width = 800;
	float height = 60;

	float x = 0; //(win_width - width) / 2.0;
	float y = (win_height - height);

    float mwidth = 800;
    float mheight = 60;
    float middle_box_x = (win_width - mwidth) / 2.0;
    float middle_box_y = (win_height - mheight) / 2.0;

    ui_theme(ctx);

    bool show_powerup = (jump_message_timer > 0.0) || (win_message_timer > 0.0);


    if(show_powerup) {
        if(nk_begin(ctx, "powerup", nk_rect(middle_box_x, middle_box_y, mwidth, mheight), NK_WINDOW_NO_SCROLLBAR)) {
            nk_layout_row_dynamic(ctx, 60, 1);
            const char *message = "you can now jump!";
            if(win_message_timer > 0.0) message = "you win!!";
            nk_label(ctx, message, NK_TEXT_CENTERED);
        }
        nk_end(ctx);
    }

	if(nk_begin(ctx, "ui", nk_rect(x, y, width, height), NK_WINDOW_NO_SCROLLBAR)) {
		nk_layout_row_dynamic(ctx, 60, 1);
        char buf[128] = {0};
        snprintf(buf, 128, "%zu/%zu carrots", got_carrot_count, carrot_count);
        nk_label(ctx, buf, NK_TEXT_LEFT);
	}
	nk_end(ctx);
}