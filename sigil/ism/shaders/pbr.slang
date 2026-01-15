
#include "common.include"

static const float  PI          = 3.141592;
static const float  Epsilon     = 0.00001;
static const float3 Fdielectric = float3(0.04);

struct vs_output_t {
    [[vk::location(0)]] float4 clip     : SV_Position;
    [[vk::location(1)]] float3 pos      : POSITION;
    [[vk::location(2)]] float3 normal   : NORMAL;
    [[vk::location(3)]] float3 color    : COLOR0;
    [[vk::location(4)]] float2 uv       : TEXCOORD;
    [[vk::location(5)]] float3 tangent  : TANGENT;
    [[vk::location(6)]] float3 bitanget : BITANGENT;
    [[vk::location(7)]] float3x3 TBN    : TEXCOORD1;
};

struct fs_output_t {
    [[vk::location(0)]] float4 color  : COLOR0;
};

struct push_constant_t {
    Ptr<vertex_t> vertices;
    uint          model;
    uint          albedo;
    uint          normal;
    uint          metal_roughness;
    uint          emissive;
    uint          ao;
};

[[vk::push_constant]] ConstantBuffer<push_constant_t> pc;

[shader("vertex")]
vs_output_t vs_main(uint vi: SV_VertexID) {
    vertex_t v = pc.vertices[vi];
    float4x4 model = get_transform(pc.model);
    float4 world_pos = mul(float4(v.position, 1.f), model);
    float4 view_pos = mul(world_pos, scene_data.view);
    float4 clip_pos = mul(view_pos, scene_data.proj);

    float3x3 model3x3 = float3x3(model[0].xyz, model[1].xyz, model[2].xyz);
    float3 world_normal = normalize(mul(v.normal, model3x3));
    float3 world_tanget = normalize(mul(v.tangent.xyz, model3x3));
    float3 world_bitangent = cross(world_normal, world_tanget) * v.tangent.w;
    float3x3 TBN = float3x3(world_tanget, world_bitangent, world_normal);

    vs_output_t out;
    out.clip = clip_pos;
    out.pos = world_pos.xyz;
    out.normal = world_normal;
    out.tangent = world_tanget;
    out.bitanget = world_bitangent;
    out.TBN = TBN;
    out.color = v.color.xyz;
    out.uv = float2(v.uv_x, v.uv_y);
    return out;
}

[shader("fragment")]
fs_output_t fs_main(vs_output_t in) {
    float3 albedo    = sample_texture(pc.albedo, in.uv).xyz;
    float  metalness = sample_texture(pc.metal_roughness, in.uv).r;
    float  roughness = sample_texture(pc.metal_roughness, in.uv).g;
    float3 tangent   = normalize(2.0 * sample_texture(pc.normal, in.uv).rgb - 1.0);
    float3 normal    = mul(tangent, in.TBN);
    float3 emissive  = sample_texture(pc.emissive, in.uv).rgb;
    float3 ao        = sample_texture(pc.ao, in.uv).xyz;

    float3 view_dir = normalize(scene_data.view_pos - in.pos);
    float3 reflect_dir = reflect(-view_dir, normal);
    float3 F0 = lerp(Fdielectric, albedo, metalness);
    float3 light_dir = normalize(scene_data.sunlight_direction.xyz);
    float3 light_color = scene_data.sunlight_color.xyz;
    float3 half_dir = normalize(light_dir + view_dir);
    float cosLo = max(0.0, dot(normal, view_dir));
    float cosLi = max(0.0, dot(normal, light_dir));
    float cosLh = max(0.0, dot(normal, half_dir));
    float cosVh = max(0.0, dot(view_dir, half_dir));
    float3 F = fresnelSchlick(F0, max(0.0, dot(half_dir, view_dir)));
    float D = ndfGGX(cosLh, roughness);
    float G = gaSchlickGXX(cosLi, cosLo, roughness);
    float3 specular_num = D * G * F;
    float specular_denom = 4.0 * cosLi * cosLo;
    float3 specular = specular_num / max(specular_denom, 0.001);
    float3 kD = (float3(1.0) - F) * (1.0 - metalness);
    float3 diffuse = kD * albedo / PI;
    float3 radiance = light_color * cosLi * 4.0;
    float3 brdf = (diffuse + specular) * radiance;
    float3 ambient = scene_data.ambient_color.rgb * albedo * ao * 0.2;
    float3 color = ambient + brdf + emissive;

    fs_output_t out;
    out.color = float4(color, 1.0);
    return out;
}

float ndfGGX(float cosLh, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = (cosLh * cosLh) * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}

float gaSchlickG1(float cosTheta, float k) {
    return cosTheta / (cosTheta * (1.0 - k) + k);
}

float gaSchlickGXX(float cosLi, float cosLo, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return gaSchlickG1(cosLi, k) * gaSchlickG1(cosLo, k);
}

float3 fresnelSchlick(float3 F0, float cosTheta) {
    return F0 + (float3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
}

