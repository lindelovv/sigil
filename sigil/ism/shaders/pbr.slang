
struct vs_output_t {
    [[vk::location(0)]] float4 clip   : SV_Position;
    [[vk::location(1)]] float3 pos    : POSITION;
    [[vk::location(2)]] float3 normal : NORMAL;
    [[vk::location(3)]] float3 color  : COLOR0;
    [[vk::location(4)]] float2 uv     : TEXCOORD;
};

struct fs_output_t {
    [[vk::location(0)]] float4 color  : COLOR0;
};

struct vertex_t {
    float3 position;
    float  uv_x;
    float3 normal;
    float  uv_y;
    float4 color;
};

struct scene_data_t {
    float4x4 view;
    float4x4 proj;
    float4x4 model;
    float4   ambient_color;
    float4   sunlight_color;
    float4   sunlight_direction;
    float3   view_pos;
    float    time;
};

struct material_data_t {
    float4 albedo_factors;
    float4 metal_roughness_factors;
};

struct push_constant_t {
    float4x4 model;
    Ptr<vertex_t> vertices;
};

[[vk::push_constant]] ConstantBuffer<push_constant_t> pc;

[[vk::binding(0, 0)]] ConstantBuffer<scene_data_t> scene_data;
[[vk::binding(1, 0)]] ConstantBuffer<material_data_t> material_data;

[[vk::binding(1, 1)]] Sampler2D albedo_texture;
[[vk::binding(2, 1)]] Sampler2D metal_roughness_texture;
[[vk::binding(3, 1)]] Sampler2D normal_texture;
[[vk::binding(4, 1)]] Sampler2D emissive_texture;
[[vk::binding(5, 1)]] Sampler2D AO_texture;

static const float  PI          = 3.141592;
static const float  Epsilon     = 0.00001;
static const float3 Fdielectric = float3(0.04);

[shader("vertex")]
vs_output_t vs_main(uint vi: SV_VertexID) {
    vertex_t v = pc.vertices[vi];
    float4 world_pos = mul(float4(v.position, 1.f), pc.model);
    float4 view_pos = mul(world_pos, scene_data.view);
    float4 clip_pos = mul(view_pos, scene_data.proj);

    vs_output_t out;
    out.clip = clip_pos;
    out.pos = world_pos.xyz;
    out.normal = mul((float3x3)pc.model, v.normal);
    out.color = v.color.xyz;
    out.uv = float2(v.uv_x, v.uv_y);
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

[shader("fragment")]
fs_output_t fs_main(vs_output_t in) {
    float3 albedo   = albedo_texture.Sample(in.uv).xyz;
    float metalness = metal_roughness_texture.Sample(in.uv).r;
    float roughness = metal_roughness_texture.Sample(in.uv).g;
    //float3 normal     = normalize(in.normal * normalize(2.0 * normal_texture.Sample(in.uv).rgb - 1.0));
    float3 normal   = normal_texture.Sample(in.uv).rgb; // TODO: normal matrix on cpu
    float emissive  = emissive_texture.Sample(in.uv).r;
    float3 AO       = AO_texture.Sample(in.uv).xyz;

    //float3 Lo = normalize(float3(scene_data.proj * scene_data.view) * scene_data.view_pos);

    //float cosLo = max(0.0, dot(normal, Lo));
    //float3 Lr = 2.0 * cosLo * normal - Lo;

    //float3 F0 = mix(Fdielectric, albedo, metalness);

    //float3 directLighting;
    //{
    //    float3 Li = scene_data.sunlight_direction.xyz * scene_data.view_pos;
    //    float3 Lh = normalize(Li + Lo);
    //    float cosLi = max(0.0, dot(normal, Li));
    //    float cosLh = max(0.0, dot(normal, Lh));
    //
    //    float3 F = fresnelSchlick(F0, max(0.0, dot(Lh, Lo)));
    //    float D = ndfGGX(cosLh, roughness);
    //    float G = gaSchlickGXX(cosLi, cosLo, roughness);
    //
    //    float3 kd = mix(float3(1.0) - F, float3(0.0), metalness);
    //    float3 diffuseBRDF = kd * albedo;
    //
    //    float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0 * cosLi * cosLo);
    //
    //    directLighting = (diffuseBRDF + specularBRDF) * float3(1.0) * cosLi;
    //}

    //float3 ambient;
    //{
    //    //float3 ambient = albedo * in.color * scene_data.ambient_color.xyz;

    //    float3 irradiance = float3(1.0); // add texture
    //    float3 F = fresnelSchlick(F0, cosLo);
    //    float3 kd = mix(float3(1.0) - F, float3(0.0), metalness);
    //    float3 diffuseIBL = kd * albedo * irradiance;

    //    //int spec_tex_lvls = textureQueryLevels(specular_texture)
    //    float2 specularBRDF = float2(0.1); //texture(specularBRDF_LUT, float2(cosLo, roughness)).rg;

    //    float3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) + float3(0.1);

    //    ambient = diffuseIBL + specularIBL;
    //}
    //_out_frag_color = float4(directLighting + ambient * normal * AO, 1.0f);

    // messy blin, something mb wrong with camera or space calculations
    float3 light_pos = scene_data.sunlight_direction.xyz;
    float3 light_color = scene_data.sunlight_color.xyz;
    float3 ambient = 0.4 * light_color;

    float3 light_dir = normalize(light_pos - in.pos);
    float diff = max(dot(normal, light_dir), 0.0);
    float3 diffuse = diff * light_color;

    float3 specular = float3(0);
    float spec_str = 0.1;
    float3 view_dir = normalize(scene_data.view_pos - in.pos);
    float3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 8);

    float ndotl = dot(normal, light_dir);
    if(ndotl > 0) {
        //float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 8);
        specular = spec_str * spec * light_color;
    }
    // cool buggy effect lol
    //float3 specular;
    //float spec_str = 0.1;
    //float3 view_dir = normalize(scene_data.view_pos - in.pos);
    //float3 reflect_dir = reflect(-light_dir, normal);
    //float ndotl = dot(normal, light_dir);
    //if(ndotl > 0) {
    //    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 8);
    //    specular = spec_str * spec * light_color;
    //}

    float3 result = (ambient + diffuse + specular /* + emissive*/) * albedo;

    fs_output_t out;
    out.color = float4(result, 1.0);

    return out;

    // maybe more correct
    //float3 n = normalize(in.normal);
    //float3 t = normalize(cross(n, float3(0, 1, 0)));
    //float3 b = cross(n, t);
    //float3x3 tbn = float3x3(t, b, n);
    //n = normalize(mul(normal, tbn));

    //float3 v = normalize(scene_data.view_pos - in.pos);

    //float3 f0 = lerp(Fdielectric, albedo, metalness);

    //float3 l = -normalize(scene_data.sunlight_direction.xyz);
    //float3 h = normalize(v + l);

    //float ndf = ndfGGX(max(dot(n, h), 0.0), roughness);
    //float g = gaSchlickGXX(max(dot(n, v), 0.0), max(dot(n, l), 0.0), roughness);
    //float3 f = fresnelSchlick(f0, max(dot(h, v), 0.0));

    //float3 ks = f;
    //float3 kd = (float3(1.0) - ks) * (1.0 - metalness);

    //float3 numerator = ndf * g * f;
    //float denominator = 4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0) + 0.001;
    //float3 specular = numerator / denominator;

    //float ndotl = max(dot(n, l), 0.0);
    //float3 radiance = scene_data.sunlight_color.rgb * scene_data.sunlight_color.a;

    //float3 lo = (kd * albedo / PI + specular) * radiance * ndotl;
    //float3 ambient = scene_data.ambient_color.rgb * albedo * ao;
    //float3 color = ambient + lo + emissive;

    //color = color / (color + float3(1.0));
    //color = pow(color, float3(1.0/2.2));

    //fs_output_t out;
    //out.color = float4(color, 1.0);
    //return out;
}

