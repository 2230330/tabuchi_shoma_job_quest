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

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
//SamplerState sampler_states[3] : register(s0);

#include"bidirectional_reflectance_distribution_function.hlsli"

float4 main(VS_OUT pin, bool is_front_face : SV_IsFrontFace) : SV_TARGET
{

    const float GAMMA = 2.2;

    const material_constants m = materials[material];
    //ベースカラー
    float4 basecolor_factor = m.pbr_metallic_roughness.basecolor_factor;
    const int basecolor_texture = m.pbr_metallic_roughness.basecolor_texture.index;
    if (basecolor_texture > -1)
    {
        float4 sampled = material_textures[BASECOLOR_TEXTURE].Sample(sampler_states[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        basecolor_factor *= sampled;
    }
#if 1
    clip(basecolor_factor.a - 0.25);
#endif

    //自己発光
    float3 emmisive_factor = m.emissive_factor;
    const int emissive_texture = m.emissive_texture.index;
    if (emissive_texture > -1)
    {
        float4 sampled = material_textures[EMISSIVE_TEXTURE].Sample(sampler_states[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        emmisive_factor *= sampled.rgb;
    }

    float roughness_factor = m.pbr_metallic_roughness.roughness_factor;
    float metallic_factor = m.pbr_metallic_roughness.metallic_factor;
    const int metallic_roughness_texture = m.pbr_metallic_roughness.metallic_roughness_texture.index;
    if (metallic_roughness_texture > -1)
    {
        float4 sampled = material_textures[METALLIC_ROUGHNESS_TEXTURE].Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
        roughness_factor *= sampled.g;
        metallic_factor *= sampled.b;
    }

    float occlusion_factor = 1.0;
    const int occlusion_texture = m.occlusion_texture.index;
    if (occlusion_texture > -1)
    {
        float4 sampled = material_textures[OCCLUSION_TEXTURE].Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
        occlusion_factor *= sampled.r;
    }
    const float occlusion_strength = m.occlusion_texture.strength;

    const float3 f0 = lerp(0.04, basecolor_factor.rgb, metallic_factor);
    const float3 f90 = 1.0;
    const float alpha_roughness = roughness_factor * roughness_factor;
    const float3 c_diff = lerp(basecolor_factor.rgb, 0.0, metallic_factor);

    const float3 P = pin.w_position.xyz;
    const float3 V = normalize(camera_position.xyz - pin.w_position.xyz);

    float3 N = normalize(pin.w_normal.xyz);
    float3 T = has_tangent ? normalize(pin.w_tangent.xyz) : float3(1, 0, 0);
    float sigma = has_tangent ? pin.w_tangent.w : 1.0;
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T) * sigma);
#if 1
	// For a back-facing surface, the tangential basis vectors are negated.
    if (is_front_face == false)
    {
        T = -T;
        B = -B;
        N = -N;
    }
#endif

    const int normal_texture = m.normal_texture.index;
    if (normal_texture > -1)
    {
        float4 sampled = material_textures[NORMAL_TEXTURE].Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
        float3 normal_factor = sampled.xyz;
        normal_factor = (normal_factor * 2.0) - 1.0;
        normal_factor = normalize(normal_factor * float3(m.normal_texture.scale, m.normal_texture.scale, 1.0));
        N = normalize((normal_factor.x * T) + (normal_factor.y * B) + (normal_factor.z * N));
    }


	// Loop for shading process for each light
//	シェーディング
    float4 color = (float4) 0;
	{
		//	環境光
        float3 ambient = ambient_color.rgb * ambient_color.a;

		//	平行光源
        float3 directional_diffuse = 0, directional_specular = 0;
		{
            float3 L = normalize(directional_light.direction.xyz);
            float3 LC = directional_light.color.rgb * directional_light.color.a;
            directional_diffuse = CalcLambert(N, L, LC, 1)*directional_light.intensity;
            directional_specular = CalcPhongSpecular(N, L, V, LC, 1)*directional_light.intensity;

			////	平行光源用シャドウマップ
   //         float depth = shadow_map.Sample(shadow_sampler_state, pin.shadow_texcoord.xy).r;
			////	深度値を比較して影かどうかを判定する
   //         if (pin.shadow_texcoord.z - depth > shadow_bias)
   //         {
   //             directional_diffuse *= shadow_attenuation;
   //             directional_specular *= shadow_attenuation;
   //         }
        }

		//	点光源
        float3 point_diffuse = 0, point_specular = 0;
        for (int i = 0; i < 8; ++i)
        {
            if (i >= light_count.y)
                break;
            
            float3 L = pin.w_position.xyz - point_light[i].position.xyz;
            float len = length(L);
            if (len >= point_light[i].range)
                continue;
            float attenuateLength = saturate(1.0f - len / point_light[i].range);
            float attenuation = attenuateLength * attenuateLength;
            L /= len;
            float3 LC = point_light[i].color.rgb * point_light[i].color.a;
            point_diffuse += CalcLambert(N, L, LC, 1) * attenuation*point_light[i].intensity;
            point_specular += CalcPhongSpecular(N, L, V, LC, 1) * attenuation*point_light[i].intensity;
        }

		//	スポットライト
        float3 spot_diffuse = 0, spot_specular = 0;
        for (int j = 0; j < 8; ++j)
        {
            if (j >= light_count.z)
                break;
            
            float3 L = pin.w_position.xyz - spot_light[j].position.xyz;
            float len = length(L);
            if (len >= spot_light[j].range)
                continue;
            float attenuateLength = saturate(1.0f - len / spot_light[j].range);
            float attenuation = attenuateLength * attenuateLength;
            L /= len;
            float3 spotDirection = normalize(spot_light[j].direction.xyz);
            float angle = dot(spotDirection, L);
            float area = spot_light[j].inner_corn - spot_light[j].outer_corn;
            attenuation *= saturate(1.0f - (spot_light[j].inner_corn - angle) / area);
            float3 LC = spot_light[j].color.rgb * spot_light[j].color.a;
            spot_diffuse += CalcLambert(N, L, LC, 1) * attenuation;
            spot_specular += CalcPhongSpecular(N, L, V, LC, 1) * attenuation;
        }
		
		//	合算
        color.a = basecolor_factor.a;
        color.rgb += basecolor_factor.rgb * (ambient + directional_diffuse + point_diffuse + spot_diffuse);
        color.rgb += directional_specular + spot_specular + point_specular;
    }
	
	//	自己発光色加算
    color.rgb += emmisive_factor;
    
    
    return color;
}