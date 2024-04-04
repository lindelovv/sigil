layout( set = 0, binding = 0 ) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec4 ambient_color;
    vec4 sunlight_dir;
    vec4 sunlight_color;
} _scene_data;

layout( set = 1, binding = 0 ) uniform GLTFMaterialData {
    vec4 color_factors;
    vec4 metal_rough_factors;
} _material_data;

layout( set = 1, binding = 1 ) uniform sampler2D _color_tex;
layout( set = 1, binding = 2 ) uniform sampler2D _metal_rough_tex;

