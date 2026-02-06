#include"deferred_rendering.hlsli"
#include"fullscreen_quad.hlsli"

Texture2D<float4> gbuffer_base_color : register(t0);
Texture2D<float4> gbuffer_emissive_color : register(t1);
Texture2D<float4> gbuffer_normal : register(t2);
Texture2D<float4> gbuffer_parameter : register(t3);
Texture2D<float> gbuffer_depth : register(t4);
Texture2D<float4> gbuffer_velocity : register(t5);

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
SamplerState sampler_states[6] : register(s0);

//  シャドウマップ用テクスチャ
Texture2D shadow_map : register(t10);
SamplerState shadow_sampler_state : register(s10);


float4 main(VS_OUT pin) : SV_TARGET
{
    //  GBufferテクスチャから情報をデコードする
    PSGBufferTextures gbuffer_textures;
    gbuffer_textures.base_color = gbuffer_base_color;
    gbuffer_textures.emissive_color = gbuffer_emissive_color;
    gbuffer_textures.normal = gbuffer_normal;
    gbuffer_textures.parameter = gbuffer_parameter;
    gbuffer_textures.depth = gbuffer_depth;
    gbuffer_textures.velocity = gbuffer_velocity;
    gbuffer_textures.state = sampler_states[POINT_WRAP];
    GBufferData data;
    data = DecodeGBuffer(gbuffer_textures, pin.texcoord, inverse_view_projection_transform, z_buffer_parameteres);


    //ライト情報を取得
    directional_lights directional_light = convert_directional_lights();
    
    float3 diffuse = (float3) 0, specular = (float3) 0;
    {
        LightingData lighting_data;
        
        float attenuation = 1.0f;
        //if (use_shadow)
        //{
        //    //シャドウマップ用のパラメータ計算
        //    float3 shadow_texcoord;
        //    {
        //        //ライトから見たNDC座標を算出
        //        float4 wvpPos = mul(float4(data.w_position.xyz, 1.0f), light_view_projection);
                
        //        //NDC座標からUV座標を算出する
        //        wvpPos /= wvpPos.w;
        //        wvpPos.y = -wvpPos.y;
        //        wvpPos.xy = 0.5f * wvpPos.xy + 0.5f;
        //        shadow_texcoord = wvpPos.xyz;
        //    }
            
        //    //深度値を比較して影かどうかを判定する
        //    float depth = shadow_map.Sample(shadow_sampler_state, shadow_texcoord.xy).r;
        //    attenuation = lerp(1, shadow_attenuation, (shadow_texcoord.z - depth > shadow_bias));
        //}
        lighting_data.direction = normalize(directional_light.direction.xyz);
        lighting_data.camera_position = camera_position.xyz;
        lighting_data.color = directional_light.color.rgb * directional_light.color.a;
        lighting_data.attenuation = attenuation;
        
        DirectLighting result = integrate_bxdf(data, lighting_data);
        diffuse += result.diffuse;
        specular += result.specular;
    }
    
    return float4(diffuse + specular, 1);
}

