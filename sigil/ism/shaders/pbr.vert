#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference     : require

#include "pbr.glsl"

layout( location = 0 ) out vec3 _out_pos;
layout( location = 1 ) out vec3 _out_normal;
layout( location = 2 ) out vec3 _out_color;
layout( location = 3 ) out vec2 _out_uv;

void main() {
    Vertex v = _pc.vertex_buffer.vertices[gl_VertexIndex];

    vec4 world_pos = vec4(v.position + _pc.pos, 1.f);
    gl_Position = _scene_data.proj * _scene_data.view * world_pos;

    _out_pos = world_pos.xyz;
    _out_normal = (gl_Position * vec4(v.normal, 0.f)).xyz;
    _out_color = v.color.xyz;
    _out_uv.x = v.uv_x;
    _out_uv.y = v.uv_y;
}

