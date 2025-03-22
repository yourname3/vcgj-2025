#version 100
precision highp float;

// both eye space.
varying vec3 v_norm;
varying vec3 v_pos;

void main() {
    gl_FragColor = vec4(1.0, 0.8, 0.5, 1.0);
}