#version 430
#extension GL_GOOGLE_include_directive : require

#include "bindless.glsl"

layout( set = 0, binding = 0 ) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 model;
    vec4 ambient_color;
    vec4 sunlight_color;
    vec4 sunlight_direction;
    vec3 view_pos;
    float time;
} _scene_data;

layout( set = 1, binding = 0 ) uniform GLTFMaterialData {
    vec4 albedo_factors;
    vec4 metal_roughness_factors;
} _material_data;

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
    mat4 model;
    VertexBuffer vertex_buffer;
} _pc;

layout( set = 1, binding = 1 ) uniform sampler2D _albedo_texture;
layout( set = 1, binding = 2 ) uniform sampler2D _metal_roughness_texture;
layout( set = 1, binding = 3 ) uniform sampler2D _normal_texture;
layout( set = 1, binding = 4 ) uniform sampler2D _emissive_texture;
layout( set = 1, binding = 5 ) uniform sampler2D _AO_texture;

