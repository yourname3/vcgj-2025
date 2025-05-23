#version 100
precision highp float;

attribute vec3 a_pos;
attribute vec3 a_norm;

// Skeleton weights. We multiply them by the matrices given to us in the array.
// (Eventually the array will be a texture).
attribute vec4 a_weight;
attribute vec4 a_weight_idx;

attribute vec2 a_uv;

varying vec3 v_norm; // Eye space
varying vec3 v_pos;

varying vec2 v_uv;

// View matrix. Used to translate stuff to eye space
uniform mat4 u_v;
// Projection matrix
uniform mat4 u_p;
// uniform mat4 u_m;

// Bone matrices.
uniform sampler2D u_skeleton;
uniform float     u_skeleton_count;

mat4
read_skeleton(float idx) {
    float y = (idx + 0.5) / u_skeleton_count;
    vec4 col0 = texture2D(u_skeleton, vec2(0.125, y));
    vec4 col1 = texture2D(u_skeleton, vec2(0.375, y));
    vec4 col2 = texture2D(u_skeleton, vec2(0.625, y));
    vec4 col3 = texture2D(u_skeleton, vec2(0.875, y));

    return mat4(col0, col1, col2, col3);
}

void main() {
    mat4 model_matrix = (read_skeleton(a_weight_idx.x) * a_weight.x)
                      + (read_skeleton(a_weight_idx.y) * a_weight.y)
                      + (read_skeleton(a_weight_idx.z) * a_weight.z)
                      + (read_skeleton(a_weight_idx.w) * a_weight.w);

    v_norm = (u_v * model_matrix * vec4(a_norm, 0.0)).xyz; // cheap approx
    
    vec4 eye_space = u_v * model_matrix * vec4(a_pos, 1.0);
    v_pos = eye_space.xyz;

    v_uv = a_uv;

    gl_Position = u_p * eye_space;
}