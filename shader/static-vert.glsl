#version 100
precision highp float;

attribute vec3 a_pos;
attribute vec3 a_norm;

// We'll need these when we implement pbr later.
// We might want to convert some of the stuff to uniforms first..?
uniform mat4 u_v;
uniform mat4 u_p;
uniform mat4 u_m;

varying vec3 v_norm;
varying vec3 v_pos;

void main() {
    mat4 vm = u_v * u_m;

    v_norm = (vm * vec4(a_norm, 0.0)).xyz;

    vec4 eye_space = vm * vec4(a_pos, 1.0);
    v_pos = eye_space.xyz;

    gl_Position = u_p * eye_space;
}
