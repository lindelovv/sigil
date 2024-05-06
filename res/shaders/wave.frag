#version 450
#extension GL_GOOGLE_include_directive : require

layout( set = 0, binding = 0 ) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec4 ambient_color;
    vec4 sunlight_direction;
    vec4 sunlight_color;
} _scene_data;

layout( set = 1, binding = 0 ) uniform GLTFMaterialData {
    vec4 albedo_factors;
    vec4 metal_roughness_factors;
} _material_data;

layout( set = 1, binding = 1 ) uniform sampler2D _albedo_texture;
layout( set = 1, binding = 2 ) uniform sampler2D _metal_roughness_texture;

layout( location = 0 ) in vec3 _in_normal;
layout( location = 1 ) in vec3 _in_color;
layout( location = 2 ) in vec2 _in_uv;

layout( location = 0 ) out vec4 _out_frag_color;

void main() {
    float light_value = max(dot(_in_normal, _scene_data.sunlight_direction.xyz), 0.1f);
    vec3 color = _in_color * texture(_albedo_texture, _in_uv).xyz;
    vec3 ambient = color * _scene_data.ambient_color.xyz;

    _out_frag_color = vec4(color * light_value * _scene_data.sunlight_color.w + ambient, 1.0f);
}

