#version 100
precision highp float;

varying vec3 v_norm;

void main() {
    vec3 norm = normalize(v_norm);
    float light = clamp(dot(norm, normalize(vec3(-0.5, -0.5, 1.0))), 0.0, 1.0);
    light += 0.2;
    light = clamp(light, 0.0, 1.0);
    vec3 color = light * vec3(0.5, 0.1, 0.9);
    gl_FragColor = vec4(color, 1.0);
}