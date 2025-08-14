#version 430
#extension GL_EXT_nonuniform_qualifier : enable

#define UNIFORM 0
#define STORAGE 1
#define SAMPLER 2

#define register_uniform(n, s)      layout(   set = 0, binding = UNIFORM) uniform  n s      get_layout_variable_name(n)[]
#define register_buffer(l, b, n, s) layout(l, set = 0, binding = STORAGE) b buffer n s      get_layout_variable_name(n)[]
#define register_sampler2d(n)       layout(   set = 0, binding = SAMPLER) uniform sampler2D get_layout_variable_name(n)[]

#define get_layout_variable_name(n) ___##n##___
#define get_resource(n, i) get_layout_variable_name(n)[nonuniformEXT(i)]

register_uniform(                 dummy_uniform, { uint ignore; });
register_buffer(std430, readonly, dummy_buffer,  { uint ignore; });
register_sampler2d(               dummy_sampler                 )

