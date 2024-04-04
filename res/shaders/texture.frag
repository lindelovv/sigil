#version 450
#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout( location = 0 ) in vec3 _in_normal;
layout( location = 1 ) in vec3 _in_color;
layout( location = 2 ) in vec2 _in_uv;

layout( location = 0 ) out vec4 _out_frag_color;

layout( set = 0, binding = 0 ) uniform sampler2D _display_texture;

void main() {
    float light_value = max(dot(_in_normal, _scene_data.sunlight_dir.xyz), 0.1f);
    vec3 color = _in_color * texture(_color_tex, _in_uv).xyz;
    vec3 ambient = color * _scene_data.ambient_color.xyz;

    _out_frag_color = vec4(color * light_value * _scene_data.sunlight_color.w + ambient, 1.f);
}

