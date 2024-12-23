#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "pbr.glsl"

layout( location = 0 ) in vec3 _in_pos;
layout( location = 1 ) in vec3 _in_normal;
layout( location = 2 ) in vec3 _in_color;
layout( location = 3 ) in vec2 _in_uv;

layout( location = 0 ) out vec4 _out_frag_color;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout( buffer_reference, std430 ) readonly buffer VertexBuffer {
    Vertex vertices[]; 
};

layout( push_constant ) uniform constants {
    VertexBuffer vertex_buffer;
    mat4 transform;
    float time;
} _push_constants;

const float PI          = 3.141592;
const float Epsilon     = 0.00001;
const vec3  Fdielectric = vec3(0.04);

float ndfGGX(float cosLh, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = (cosLh * cosLh) * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}

float gaSchlickG1(float cosTheta, float k) {
    return cosTheta / (cosTheta * (1.0 - k) + k);
}

float gaSchlickGXX(float cosLi, float cosLo, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return gaSchlickG1(cosLi, k) * gaSchlickG1(cosLo, k);
}

vec3 fresnelSchlick(vec3 F0, float cosTheta) {
    return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    vec3 albedo     = texture(_albedo_texture, _in_uv).xyz;
    float metalness = texture(_metal_roughness_texture, _in_uv).r;
    float roughness = texture(_metal_roughness_texture, _in_uv).g;
    vec3 normal     = normalize(_in_normal * normalize(2.0 * texture(_normal_texture, _in_uv).rgb - 1.0));
    float emissive  = texture(_emissive_texture, _in_uv).r;
    vec3 AO         = texture(_AO_texture, _in_uv).xyz;

    vec3 Lo = normalize(vec3(_scene_data.proj * _scene_data.view) * _in_pos);

    float cosLo = max(0.0, dot(normal, Lo));
    vec3 Lr = 2.0 * cosLo * normal - Lo;

    vec3 F0 = mix(Fdielectric, albedo, metalness);

    vec3 directLighting;
    {
        vec3 Li = _scene_data.sunlight_direction.xyz;
        vec3 Lh = normalize(Li + Lo);
        float cosLi = max(0.0, dot(normal, Li));
        float cosLh = max(0.0, dot(normal, Lh));
    
        vec3 F = fresnelSchlick(F0, max(0.0, dot(Lh, Lo)));
        float D = ndfGGX(cosLh, roughness);
        float G = gaSchlickGXX(cosLi, cosLo, roughness);
    
        vec3 kd = mix(vec3(1.0) - F, vec3(0.0), metalness);
        vec3 diffuseBRDF = kd * albedo;
    
        vec3 specularBRDF = (F * D * G) / max(Epsilon, 4.0 * cosLi * cosLo);
    
        directLighting = (diffuseBRDF + specularBRDF) * vec3(1.0) * cosLi;
    }

    vec3 ambient;
    {
        //vec3 ambient = albedo * _in_color * _scene_data.ambient_color.xyz;

        vec3 irradiance = vec3(1.0); // add texture
        vec3 F = fresnelSchlick(F0, cosLo);
        vec3 kd = mix(vec3(1.0) - F, vec3(0.0), metalness);
        vec3 diffuseIBL = kd * albedo * irradiance;

        //int spec_tex_lvls = textureQueryLevels(specular_texture)
        vec2 specularBRDF = vec2(0.1); //texture(specularBRDF_LUT, vec2(cosLo, roughness)).rg;

        vec3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) + vec3(0.1);

        ambient = diffuseIBL + specularIBL;
    }

    _out_frag_color = vec4(directLighting + ambient * albedo * AO, 1.0f);
}

