#version 450

layout( location = 0 ) in vec3 _in_color;
layout( location = 1 ) in vec2 _in_uv;

layout( location = 0 ) out vec4 _out_frag_color;

layout( set = 0, binding = 0 ) uniform sampler2D _display_texture;

void main() {
    _out_frag_color = texture(_display_texture, _in_uv);
}

