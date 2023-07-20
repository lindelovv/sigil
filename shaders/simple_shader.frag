#version 450

layout ( binding = 1 ) uniform sampler2D texture_sampler;

layout ( location = 0 ) in vec3 frag_color;
layout ( location = 1 ) in vec2 frag_texture_coords;

layout ( location = 0 ) out vec4 out_color;

void main() {
    out_color = texture(texture_sampler, frag_texture_coords * 4.f);
}

