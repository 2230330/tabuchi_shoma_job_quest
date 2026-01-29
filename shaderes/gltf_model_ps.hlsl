#include"../shaderes/gltf_model.hlsli"
#include "scene_constant_buffer.hlsli"
#include"forward_light.hlsli"
#include"shading_functions.hlsli"

struct texture_info
{
    int index; // required.
    int texcoord; // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
};
struct normal_texture_info
{
    int index; // required
    int texcoord; // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
    float scale; // scaledNormal = normalize((<sampled normal texture value> * 2.0 - 1.0) * vec3(<normal scale>, <normal scale>, 1.0))
};
struct occlusion_texture_info
{
    int index; // required
    int texcoord; // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
    float strength; // A scalar parameter controlling the amount of occlusion applied. A value of `0.0` means no occlusion. A value of `1.0` means full occlusion. This value affects the final occlusion value as: `1.0 + strength * (<sampled occlusion texture value> - 1.0)`.
};
struct pbr_metallic_roughness
{
    float4 basecolor_factor; // len = 4. default [1,1,1,1]
    texture_info basecolor_texture;
    float metallic_factor; // default 1
    float roughness_factor; // default 1
    texture_info metallic_roughness_texture;
};
struct material_constants
{
    float3 emissive_factor; // length 3. default [0, 0, 0]
    int alpha_mode; // "OPAQUE" : 0, "MASK" : 1, "BLEND" : 2
    float alpha_cutoff; // default 0.5
    int double_sided; // default false;

    pbr_metallic_roughness pbr_metallic_roughness;

    normal_texture_info normal_texture;
    occlusion_texture_info occlusion_texture;
    texture_info emissive_texture;
};
StructuredBuffer<material_constants> materials : register(t0);

#define BASECOLOR_TEXTURE 0
#define METALLIC_ROUGHNESS_TEXTURE 1
#define NORMAL_TEXTURE 2
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4
Texture2D<float4> material_textures[5] : register(t1);

//キューブマップ情報
TextureCube cubemap_texture : register(t100);

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
SamplerState sampler_states[5] : register(s0);

float4 main(VS_OUT pin, bool is_front_face : SV_IsFrontFace) : SV_TARGET
{
    material_constants m = materials[material];
    
    //ベースカラーを取得
    float4 base_color = m.pbr_metallic_roughness.basecolor_factor;
    if (m.pbr_metallic_roughness.basecolor_texture.index > -1)
    {
        float4 sampled = material_textures[BASECOLOR_TEXTURE].Sample(sampler_states[ANISOTROPIC], pin.texcoord);
        //リニア空間へ
        sampled.rgb = pow(sampled.rgb, GammaFactor);
        base_color *= sampled;
    }
    //自己発光色を取得
    float3 emisive_color = m.emissive_factor;
    if (m.emissive_texture.index > -1)
    {
        float3 emisive = material_textures[EMISSIVE_TEXTURE].Sample(sampler_states[ANISOTROPIC], pin.texcoord).rgb;
        emisive.rgb = pow(emisive.rgb, GammaFactor);
        emisive_color.rgb *= emisive.rgb;
    }
    //法線/従法線/接戦
    float3 N = normalize(pin.w_normal.xyz);
    float3 T = has_tangent ? normalize(pin.w_tangent.xyz) : float3(1, 0, 0);
    float sigma = has_tangent ? pin.w_tangent.w : 1.0f;
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T) * sigma);
    //裏面描画の場合は反転しておく
    if (is_front_face == false)
    {
        T = -T;
        B = -B;
        N = -N;
    }
    //法線マッピング
    if (m.normal_texture.index > -1)
    {
        float4 sampled = material_textures[NORMAL_TEXTURE].Sample(sampler_states[LINEAR_WRAP], pin.texcoord);
        float3 normal_factor = sampled.xyz;
        normal_factor = (normal_factor * 2.0f) - 1.0f;
        normal_factor = normalize(normal_factor * float3(m.normal_texture.scale, m.normal_texture.scale, 1.0f));
        N = normalize((normal_factor.x * T) + (normal_factor.y * B) + (normal_factor.z * N));
    }
    //金属質・粗さを取得
    float roughness = m.pbr_metallic_roughness.roughness_factor;
    float metalness = m.pbr_metallic_roughness.metallic_factor;
    if (m.pbr_metallic_roughness.metallic_roughness_texture.index > -1)
    {
        float4 sampled = material_textures[METALLIC_ROUGHNESS_TEXTURE].Sample(sampler_states[LINEAR_WRAP], pin.texcoord);
        roughness *= sampled.g;
        metalness *= sampled.b;
    }

    //光の遮蔽値を取得
    float occlusion_factor = 1.0f;
    if (m.occlusion_texture.index > -1)
    {
        float4 sampled = material_textures[OCCLUSION_TEXTURE].Sample(sampler_states[LINEAR_WRAP], pin.texcoord);
        occlusion_factor *= sampled.r;
    }
    const float occlusion_strength = m.occlusion_texture.strength;
    //非金属部
    float4 albedo = base_color;
    //入射光のうち拡散反射になる割合
    float3 diffuse_reflectance = lerp(albedo.rgb, 0.0f, metalness);
    //垂直反射時のフレネル反射率(非金属でも最低4％は鏡面反射する)
    float3 F0 = lerp(0.04f, albedo.rgb, metalness);
    //視線ベクトル
    //float3 V = normalize(pin.w_position.xyz - camera_position.xyz);
    float3 V = normalize(camera_position.xyz - pin.w_position.xyz);

    //直接光のシューディング
    float3 total_diffuse = 0, total_specular = 0;
    {
        //平行光源の処理
        {
            float3 diffuse = (float3) 0, specular = (float3) 0;
            float3 L = normalize(directional_light.direction.xyz);
            float3 LC = directional_light.color.rgb*directional_light.color.w;
            DirectBRDF(diffuse_reflectance, F0, N, V, L, LC, roughness, diffuse, specular);

            total_diffuse += diffuse;
            total_specular += specular;
        }
        //点光源
        for (int i = 0; i < 8; ++i)
        {
            if (i >= light_count.y)
            {
                break;
            }
            float3 L = pin.w_position.xyz - point_light[i].position.xyz;
            float len = length(L);
            if (len >= point_light[i].range)
            {
                continue;
            }
            float attenuateLength = saturate(1.0f - len / point_light[i].range);
            float attenuation = attenuateLength * attenuateLength;
            L /= len;
            float3 LC = point_light[i].color.rgb *point_light[i].intensity;
            
            float3 diffuse = (float3) 0, specular = (float3) 0;
            DirectBRDF(diffuse_reflectance, F0, N, V, L, LC * attenuation, roughness, diffuse, specular);
            total_diffuse += diffuse;
            total_specular += specular;
        }
        //スポットライト
        for (int j = 0; j < 8; ++j)
        {
            if (j >= light_count.z)
            {
                break;
            }
            
            float3 L = pin.w_position.xyz - spot_light[j].position.xyz;
            float len = length(L);
            if (len >= spot_light[j].range)
            {
                continue;
            }
            float attenuateLength = saturate(1.0f - len / spot_light[j].range);
            float attenuation = attenuateLength * attenuateLength;
            L /= len;
            float3 spotDirection = normalize(spot_light[j].direction.xyz);
            float angle = dot(spotDirection, L);
            float area = spot_light[j].inner_corn - spot_light[j].outer_corn;
            attenuation *= saturate(1.0f - (spot_light[j].inner_corn - angle) / area);
            float3 LC = spot_light[j].color.rgb * spot_light[j].color.a;
            
            float3 diffuse = (float3) 0, specular = (float3) 0;
            DirectBRDF(diffuse_reflectance, F0, N, V, L, LC * attenuation, roughness, diffuse, specular);
            total_diffuse += diffuse;
            total_specular += specular;
        }
        

        // 視線（ピクセル→カメラ）※修正済みのVを使う
        float3 R = reflect(-V, N); // 反射方向

        // roughnessでmipを選ぶ（ざっくり）
        // ※ cubemapにmipがある前提（prefilter済みが理想）
        float mip = roughness * 8.0f; // 8.0f は mip段数に合わせて調整

        float3 env = cubemap_texture.SampleLevel(sampler_states[LINEAR_CLAMP], R, mip).rgb;

        // Fresnel（角度依存で反射が強くなる）
        float NdotV = saturate(dot(N, V));
        float3 F = CalcFresnel(F0, NdotV);

        // 金属は強く反射、非金属は弱く（それっぽく）
        float reflect_strength = lerp(0.04f, 1.0f, metalness);

        // 反射をspecularへ加算
        total_specular += env * F * reflect_strength;

    }
    

    
    //遮蔽処理
    total_diffuse = lerp(total_diffuse, total_diffuse * occlusion_factor, occlusion_strength);
    total_specular = lerp(total_specular, total_specular * occlusion_factor, occlusion_strength);
    
    //色生成
    float3 color = total_diffuse + total_specular + (emisive_color);
    
    
    //sRGB空間へ
    color.rgb = pow(color.rgb, 1.0f / GammaFactor);
    return float4(color, base_color.a);
}