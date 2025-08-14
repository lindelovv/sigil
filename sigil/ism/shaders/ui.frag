#version 450

layout( location = 0 ) in vec3 _in_pos;
layout( location = 1 ) in vec3 _in_normal;
layout( location = 2 ) in vec3 _in_color;
layout( location = 3 ) in vec2 _in_uv;

layout( location = 0 ) out vec4 _out_frag_color;

void main() {
    _out_frag_color = vec4(.2f);
}

