#include "common.include"

struct push_constant_t {
    Ptr<vertex_t> vertices;
    uint          model;
};

[[vk::push_constant]] ConstantBuffer<push_constant_t> pc;

[shader("vertex")]
float4 vs_main(uint vi: SV_VertexID) : SV_Position {
    vertex_t v = pc.vertices[vi];
    float4x4 model = get_transform(pc.model);
    float4 world_pos = mul(model, float4(v.position, 1.0));
    float4 light_view_pos = mul(scene_data.light_view, world_pos);
    float4 light_clip_pos = mul(scene_data.light_proj, light_view_pos);
    return light_clip_pos;
}

