#version 330

#define PI 3.1415926538

// Eye-space.
in vec3 fPos;
in vec3 fNormal;
in vec2 fTexCoord;
in vec3 fTangent;
in vec3 fBitangent;

uniform mat3 uNormalMatrix;

uniform struct Material {
    vec3 base_color;
    float metallic;
    float perceptual_roughness;
    float reflectance;
    
    bool use_base_color_map;
    sampler2D base_color_map;
    
    bool use_metallic_roughness_map;
    sampler2D metallic_roughness_map;
    
    bool use_normal_map;
    sampler2D normal_map;
    
    bool use_emissive_map;
    sampler2D emissive_map;
    
    bool use_occlusion_map;
    sampler2D occlusion_map;
} material;

uniform samplerCube environment_map;
uniform bool use_environment_map;

struct Light {
    // Eye-space.
    vec3 direction;
    vec3 color;
};
const int MAX_LIGHTS = 5;
uniform Light lights[ MAX_LIGHTS ];

uniform int num_lights;

uniform int num_samples_diffuse;
uniform int num_samples_specular;

uniform float uTime;

// gl_FragColor is old-fashioned, but it's what WebGL 1 uses.
// From: https://stackoverflow.com/questions/9222217/how-does-the-fragment-shader-know-what-variable-to-use-for-the-color-of-a-pixel
layout(location = 0) out vec4 FragColor;

// Your code goes here.

// Returns a rotation matrix that applies the rotation given by the axis and angle.
mat3 rotation_matrix_from_axis_sin_cos_angle( vec3 axis, float sin_angle, float cos_angle ) {
    float u1 = axis.x;
    float u2 = axis.y;
    float u3 = axis.z;
    
    mat3 U = transpose(mat3(
        u1*u1, u1*u2, u1*u3,
        u2*u1, u2*u2, u2*u3,
        u3*u1, u3*u2, u3*u3
    ));
    mat3 S = transpose(mat3(
        0, -u3, u2,
        u3,  0, -u1,
        -u2,  u1, 0
    ));
    mat3 result = U + cos_angle * (mat3(1.0) - U) + sin_angle * S;
    return result;
}
mat3 rotation_matrix_from_axis_angle( vec3 axis, float angle ) {
    return rotation_matrix_from_axis_sin_cos_angle( axis, sin(angle), cos(angle) );
}

/* D_GGX, F_Schlick, V_SmithGGXCorrelated, Fd_Lambert copied from
 * https://google.github.io/filament/Filament.md.html#materialsystem/standardmodelsummary
 *
 * Or from the assignment description.
 */
float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

vec3 F_Schlick(float u, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

float Fd_Lambert() {
    return 1.0 / PI;
}

/* Define data types used to pass information in and out of the main BRDF */
struct BrdfIn {
    vec3 diffuse_color;
    vec3 f0;
    float roughness;

    vec3 n;
    vec3 l;
    vec3 v;
};

struct BrdfOut {
    float NoV;
    float NoL;
    float NoH;
    float LoH;

    float D;
    vec3  F;
    float V;

    vec3 Fd;
    vec3 Fr;

    vec3 brdf;

    vec3 h;
};

/* Computes the specular component of the BRDF given an h vector. This turned
 * out to not quite be necessary perhaps. */
BrdfOut BRDF_Fr_h(BrdfIn bin, vec3 h) {
    vec3 v = bin.v;
    vec3 n = bin.n;
    vec3 l = bin.l;

    BrdfOut bout;
    bout.h = h;

    bout.NoV = abs(dot(n, v)) + 1e-5;
    bout.NoL = clamp(dot(n, l), 0.0, 1.0);
    bout.NoH = clamp(dot(n, h), 0.0, 1.0);
    bout.LoH = clamp(dot(l, h), 0.0, 1.0);

    bout.D = D_GGX(bout.NoH, bin.roughness);
    bout.F = F_Schlick(bout.LoH, bin.f0);
    bout.V = V_SmithGGXCorrelated(bout.NoV, bout.NoL, bin.roughness);
    
    // Specular BRDF
    // Note that this formula multiplies by D even though we often need to
    // divide by D. This is not the most efficient--we could simply delete the
    // multiplication and division in that case--but to ensure that all the
    // shaders are using the same BRDF I did it this way for now.
    bout.Fr = (bout.D * bout.V) * bout.F;

    return bout;
}

/* Computes the specular part of the BRDF given the "BrdfIn". */
BrdfOut BRDF_Fr(BrdfIn bin) {
    return BRDF_Fr_h(bin, normalize(bin.v + bin.l));
}

/* Computes the entire BRDF given the "BrdfIn". */
BrdfOut BRDF(BrdfIn bin) {
    BrdfOut bout = BRDF_Fr(bin);
    // diffuse BRDF
    bout.Fd = bin.diffuse_color * Fd_Lambert();
    return bout;
}

/*
Call `hammersley()` with `i` from 0 to `N-1` to get points evenly distributed on the unit square [0,1)^2.
Source: <https://web.archive.org/web/20230412142209/http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html>
*/
vec2 hammersley(uint i, uint N)
{
    uint bits = i;
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float y = float(bits) * 2.3283064365386963e-10; // / 0x100000000
    
    return vec2(float(i)/float(N), y);
}

/* Computes a single diffuse sample for sampled image-based lighting. */
vec3 Fd_sampled(BrdfIn bin, vec2 rng, mat3 rot) {
    float phi = 2 * PI * rng.x;
    float theta = acos(1.0 - rng.y);

    vec3 dir = vec3(
        cos(phi) * sin(theta),
        sin(phi) * sin(theta),
        cos(theta)
    );

    // Dir is now in eye space.
    dir = rot * dir;

    vec3 color = textureLod(environment_map, dir, 0).rgb;
    float cos_theta_i = clamp(dot(dir, bin.n), 0.0, 1.0);

    // This directly computes the diffuse part of our BRDF instead of using
    // the BRDF function--this is just because the diffuse BRDF is quite simple.
    return (bin.diffuse_color / PI) * color * cos_theta_i;
}

/* Computes a single specular sample for sampled image-based lighting. */
vec3 Fr_sampled(BrdfIn bin, vec2 rng, mat3 rot) {
    float phi = 2 * PI * rng.x;
    float theta = acos(sqrt(
        (1 - rng.y) /
        (rng.y * ((bin.roughness * bin.roughness) - 1.0) + 1.0)
    ));

    vec3 h = vec3(
        cos(phi) * sin(theta),
        sin(phi) * sin(theta),
        cos(theta)
    );

    // Half vector is now in eye space
    h = rot * h;
    
    // Set light direction for our BRDF bin parameter based on the generated
    // half vector.
    bin.l = reflect(-bin.v, h);
    bin.l = normalize(bin.l);

    vec3 Il = textureLod(environment_map, bin.l, 0).rgb;

    // Use the same BRDF function to be sure everything is as it should be.
    BrdfOut b = BRDF_Fr(bin);

    float VoH = dot(bin.v, b.h);

    // Correct formula is:
    // 4 * f_r * L_i * cos(theta_i) * (v dot h) / D * cos(theta_h)
    // This formula was worked out based on the derivation here:
    //    https://www.mathematik.uni-marburg.de/~thormae/lectures/graphics1/graphics_10_2_eng_web.html#32
    // Except applied to our original formula:
    //    f_r(p, w_i -> w_o) * L_i * cos(theta_i)
    // This gives the VoH and 4 terms in the numerator and results in the cancellation
    // of the sine term in the denominator.
    return 4.0 * b.Fr * Il * b.NoL * VoH / (b.D * cos(theta));
}

/* Computes the BRDF attenuated by N dot L. Useful for directional lights. */
vec3 BRDF_attenuated(BrdfIn bin) {
    BrdfOut b = BRDF(bin);
    return (b.Fd + b.Fr) * b.NoL;
}

/* Computes the IBL based on the environment_map texture. */
vec3 BRDF_envmap(BrdfIn bin) {
    float max_level = 1.0 + floor(log2(textureSize(environment_map, 0).x));

    // Initialize Fd to 0 in case we do the sum
    vec3 Fd = vec3(0.0);
    if(num_samples_diffuse > 0) {
        // Create a rotation matrix for rotating the hemisphere of points to align
        // with the normal.
        mat3 rot = rotation_matrix_from_axis_angle(
            normalize(cross(vec3(0.0, 0.0, 1.0), bin.n)),
            acos(dot(vec3(0.0, 0.0, 1.0), bin.n))
        );
        for(int i = 0; i < num_samples_diffuse; ++i) {
            vec2 rng = hammersley(uint(i), uint(num_samples_diffuse));
            Fd += Fd_sampled(bin, rng, rot);
        }
        Fd *= (2 * PI) / float(num_samples_diffuse);
    }
    else {
        // Direct lookup
        Fd = bin.diffuse_color * textureLod(environment_map, bin.n, max_level - 1).rgb;
    }

    // For direct image-based specular, use the reflected view vector as the light
    // vector.
    bin.l = reflect(-bin.v, bin.n);

    // Use roughness^1/8 instead of perceptual_roughness^1/4
    // Saves one parameter.
    vec3 Il = textureLod(environment_map, bin.l, (max_level - 1) * pow(bin.roughness, 0.125)).rgb;

    // Initialize Fr to 0 in case we do the sum
    vec3 Fr = vec3(0.0); 
    if(num_samples_specular > 0) {
        // Create a rotation matrix for rotating the hemisphere of points to align
        // with the normal.
        mat3 rot = rotation_matrix_from_axis_angle(
            normalize(cross(vec3(0.0, 0.0, 1.0), bin.n)),
            acos(dot(vec3(0.0, 0.0, 1.0), bin.n))
        );
        for(int i = 0; i < num_samples_specular; ++i) {
            vec2 rng = hammersley(uint(i), uint(num_samples_specular));
            vec3 current = Fr_sampled(bin, rng, rot);
            Fr += current;
        }
        Fr /= float(num_samples_specular);
    }
    else {
        // Direct lookup
        BrdfOut b = BRDF_Fr(bin);
        Fr = PI * b.Fr * Il * b.NoL / b.D;
    }

    return Fd + Fr;
}

void main()
{
    float metallic = material.metallic;
    float perceptual_roughness = material.perceptual_roughness;
    // Compute metallic and perceptual_roughness based on textures
    if(material.use_metallic_roughness_map) {
        vec4 v = texture2D(material.metallic_roughness_map, fTexCoord);
        metallic *= v.b;
        perceptual_roughness *= v.g;
    }
    float reflectance = material.reflectance;

    // Compute base_color based on textures
    vec3 base_color = material.base_color;
    if(material.use_base_color_map) {
        base_color *= texture2D(material.base_color_map, fTexCoord).rgb;
    }

    // Compute BRDF parameters
    vec3 diffuse_color = (1.0 - metallic) * base_color;
    vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + base_color * metallic;
    float roughness = clamp( perceptual_roughness * perceptual_roughness, 0.01, 1.0 );

    vec3 normal = normalize(fNormal);
    if(material.use_normal_map) {
        // Compute the weights for u, v, n
        vec3 weights = texture2D(material.normal_map, fTexCoord).rgb;
        weights = (weights - 0.5) * 2.0;

        // Compute the weighted sum. (Essentially a change of basis)
        normal = weights.x * normalize(fTangent)
            + weights.y * normalize(fBitangent)
            + weights.z * normal; // "normal" already normalized
        normal = normalize(normal);
    }

    if(material.use_occlusion_map) {
        // Scale diffuse_color by the ambient occlusion as it should only
        // scale the diffuse BRDF.
        diffuse_color *= texture2D(material.occlusion_map, fTexCoord).r;
    }

    // Set up the input parameters for BRDF computation.
    BrdfIn bin;
    bin.diffuse_color = diffuse_color;
    bin.f0 = f0;
    bin.roughness = roughness;
    bin.v = -normalize(fPos);
    bin.n = normal;

    // Sum of all radiance
    vec3 sum = vec3(0.0);

    for(int i = 0; i < num_lights; ++i) {
        // Each directional light simply changes L and uses BRDF_attenuated
        bin.l = -normalize(lights[i].direction);
        vec3 brdf = BRDF_attenuated(bin);
        sum += brdf * lights[i].color;
    }

    if(use_environment_map) {
        // Image based lighting all handled here
        sum += BRDF_envmap(bin);
    }

    // Add emission
    if(material.use_emissive_map) {
        sum += texture2D(material.emissive_map, fTexCoord).rgb;
    }

    // Done
    FragColor = vec4(clamp(sum, 0.0, 1.0), 1.0);
}
