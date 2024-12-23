layout( set = 0, binding = 0 ) uniform SceneData {
    mat4 view;
    mat4 proj;
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
layout( set = 1, binding = 3 ) uniform sampler2D _normal_texture;
layout( set = 1, binding = 4 ) uniform sampler2D _emissive_texture;
layout( set = 1, binding = 5 ) uniform sampler2D _AO_texture;

