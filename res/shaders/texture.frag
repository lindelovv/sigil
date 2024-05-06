#version 450
#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout( location = 0 ) in vec3 _in_normal;
layout( location = 1 ) in vec3 _in_color;
layout( location = 2 ) in vec2 _in_uv;

layout( location = 0 ) out vec4 _out_frag_color;

const float PI = 3.141592;
const float Epsilon = 0.00001;

void main() {
    vec3 albedo     = texture(_albedo_texture, _in_uv).xyz;
    float metalness = texture(_metal_roughness_texture, _in_uv).r;
    vec3 normal     = normalize(_in_normal * normalize(2.0 * texture(_normal_texture, _in_uv).rgb - 1.0));
    float emissive  = texture(_emissive_texture, _in_uv).r;
    vec3 AO         = texture(_AO_texture, _in_uv).xyz;

    float light_value = max(dot(normal, _scene_data.sunlight_direction.xyz), 0.1f);
    vec3 ambient = albedo * _in_color * _scene_data.ambient_color.xyz;

    _out_frag_color = vec4(albedo * AO * light_value * _scene_data.sunlight_color.w + ambient, 1.0f);
}

