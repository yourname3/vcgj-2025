#version 100
precision highp float;

attribute vec3 a_pos;
attribute vec3 a_norm;

// Skeleton weights. We multiply them by the matrices given to us in the array.
// (Eventually the array will be a texture).
attribute vec4 a_weight;
attribute vec4 a_weight_idx;

varying vec3 v_norm;

uniform mat4 u_vp;
// uniform mat4 u_m;

// Bone matrices.
uniform mat4 u_skeleton[4];

void main() {
    mat4 model_matrix = (u_skeleton[int(a_weight_idx.x)] * a_weight.x)
                      + (u_skeleton[int(a_weight_idx.y)] * a_weight.y)
                      + (u_skeleton[int(a_weight_idx.z)] * a_weight.z)
                      + (u_skeleton[int(a_weight_idx.w)] * a_weight.w);

    v_norm = (model_matrix * vec4(a_norm, 0.0)).xyz; // cheap approx
    gl_Position = u_vp * model_matrix * vec4(a_pos, 1.0);
}