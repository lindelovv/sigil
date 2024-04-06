#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "input_structures.glsl"

layout( location = 0 ) out vec3 _out_normal;
layout( location = 1 ) out vec3 _out_color;
layout( location = 2 ) out vec2 _out_uv;

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
    mat4 render_matrix;
    VertexBuffer vertex_buffer;
    float time;
} _push_constants;

const float PI = 3.14285714286;

void main() {
    Vertex v = _push_constants.vertex_buffer.vertices[gl_VertexIndex];

    vec3 pos = v.position;
    float distance = length(v.position);
    float wave = sin(3.0 * PI * distance * 0.3 + _push_constants.time) * 0.5;

    pos.z += wave;

    gl_Position = _push_constants.render_matrix * vec4(pos, 1.f);

    _out_normal = (_push_constants.render_matrix * vec4(v.normal, 0.f)).xyz;
    _out_color = v.color.xyz * _material_data.color_factors.xyz;
    _out_uv.x = v.uv_x;
    _out_uv.y = v.uv_y;
}

