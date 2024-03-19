#version 450

layout ( binding = 0 ) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout ( location = 0 ) in vec3 in_position;
layout ( location = 1 ) in vec3 in_color;
layout ( location = 2 ) in vec2 in_texture_coords;

layout ( location = 0 ) out vec3 frag_color;
layout ( location = 1 ) out vec2 frag_texture_coords;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(in_position, 1.0);
    frag_color = in_color;
    frag_texture_coords = in_texture_coords;
}

