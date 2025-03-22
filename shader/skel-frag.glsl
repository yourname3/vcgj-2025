#version 100
precision highp float;

// both eye space.
varying vec3 v_norm;
varying vec3 v_pos;

#define PI 3.1415926538

float
Fd_Lambert() {
    return 1.0 / PI;
}

float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

vec3
F_Schlick(float u, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

float
V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

vec3
brdf_attenuated(vec3 diffuse_color, vec3 f0, float roughness, vec3 n, vec3 l, vec3 v) {
    vec3 h = normalize(v + l);
    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    float D = D_GGX(NoH, roughness);
    vec3 F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness);

    vec3 Fd = diffuse_color * Fd_Lambert();
    vec3 Fr = (D * V) * F;

    return (Fd + Fr) * NoL;
}

vec3
brdf_envmap(vec3 diffuse_color, vec3 f0, float roughness, vec3 n, vec3 l, vec3 v) {
    

    // Use this if we get an env map.
    //float max_level = 1.0 + floor(log2(textureSize(environment_map, 0).x));
    //vec3 sampled_Id = textureLod(environment_map, n, max_level - 1).rgb;
    // Use roughness^1/8 instead of perceptual_roughness^1/4
    // Saves one parameter.
    //vec3 sampled_Il = textureLod(environment_map, l, (max_level - 1) * pow(roughness, 0.125)).rgb;

    // Otherwise, just do constant color.
    vec3 sampled_Id = vec3(0.3, 0.5, 0.9) * 0.5;
    vec3 sampled_Il = sampled_Id;

    // Initialize Fd to 0 in case we do the sum
    vec3 Fd = vec3(0.0);
    // Direct lookup
    Fd = diffuse_color * sampled_Id;

    // For direct image-based specular, use the reflected view vector as the light
    // vector.
    l = reflect(-v, n);

    vec3 h = normalize(v + l);
    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    float D = D_GGX(NoH, roughness);
    vec3 F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness);

    vec3 Fr = (D * V) * F;

    Fr = 4.0 * NoV * Fr * sampled_Il * NoL / D;

    return Fd + Fr;
}

void main() {
    vec3 normal = normalize(v_norm);

    float metallic = 0.0;
    float perceptual_roughness = 0.8;

    float reflectance = 0.5;

    vec3 base_color = vec3(0.8, 0.1, 0.9);
    
    vec3 diffuse_color = (1.0 - metallic) * base_color;
    vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + base_color * metallic;
    float roughness = clamp(perceptual_roughness * perceptual_roughness, 0.01, 1.0);

    vec3 sum = vec3(0.0);

    vec3 light_direction = vec3(0, -1, -1);
    vec3 v = -normalize(v_pos);

    sum = brdf_attenuated(
        diffuse_color,
        f0,
        roughness,
        normal,
        -normalize(light_direction),
        v_pos
    );

    sum += brdf_envmap(
        diffuse_color,
        f0,
        roughness,
        normal,
        -normalize(light_direction),
        v_pos
    );

    gl_FragColor = vec4(clamp(sum, 0.0, 1.0), 1.0);
}