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
    float3 diffuse = (float3) 0, specular = (float3) 0;
    LightingData lighting_data;
    switch (get_light_kinds())
    {
        case light_kind_directional:
            directional_lights directional_light = convert_directional_lights();
            {
        
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
            break;
        case light_kind_point_light:
            point_lights point_light = convert_point_lights();
        
            if (data.shading_model != shading_model_unlit)
            {
                float3 L = data.w_position.xyz - point_light.position.xyz;
                float len = length(L);
                if (len < point_light.range)
                {
                    L /= len;
            
                    float attenuateLength = saturate(1.0f - len / point_light.range);
                    float attenuation = attenuateLength * attenuateLength;
            
                    LightingData lighting_data;
                    lighting_data.attenuation = attenuation;
                    lighting_data.direction = L;
                    lighting_data.color = point_light.color.rgb * point_light.color.a;
                    lighting_data.camera_position = camera_position.xyz;
                    DirectLighting result = integrate_bxdf(data, lighting_data);
                    diffuse += result.diffuse;
                    specular += result.specular;
                }
            }
            break;
        case light_kind_spot_light:
            spot_lights spot_light = convert_spot_lights();
        
            if (data.shading_model != shading_model_unlit)
            {
                float3 L = data.w_position.xyz - spot_light.position.xyz;
                float len = length(L);
                if (len < spot_light.range)
                {
                    float attenuateLength = saturate(1.0f - len / spot_light.range);
                    float attenuation = attenuateLength * attenuateLength;
                    L /= len;
                    float3 spotDirection = normalize(spot_light.direction.xyz);
                    float innerCorn = cos(spot_light.inner_corn);
                    float outerCorn = cos(spot_light.outer_corn);
                    float angle = dot(spotDirection, L);
                    float area = innerCorn - outerCorn;
                    attenuation *= saturate(1.0f - (innerCorn - angle) / area);
                  //  if (use_shadow)
                  //  {
	                 //   //  シャドウマップ用のパラメーター計算
                  //      float3 shadow_texcoord;
	                 //   {
		                //    // ライトから見たNDC座標を算出
                  //          float4 wvpPos = mul(float4(data.w_position.xyz, 1.0f), light_view_projection);
		                //    // NDC座標からUV座標を算出する
                  //          wvpPos /= wvpPos.w;
                  //          wvpPos.y = -wvpPos.y;
                  //          wvpPos.xy = 0.5f * wvpPos.xy + 0.5f;
                  //          shadow_texcoord = wvpPos.xyz;
                  //      }
		                ////	シャドウマップ
                  //      float depth = shadow_map.Sample(shadow_sampler_state, shadow_texcoord.xy).r;
		                ////	深度値を比較して影かどうかを判定する
                  //      if (shadow_texcoord.z - depth > shadow_bias)
                  //      {
                  //          attenuation *= shadow_attenuation;
                  //      }
                  //  }

                    LightingData lighting_data;
                    lighting_data.attenuation = attenuation;
                    lighting_data.direction = L;
                    lighting_data.color = spot_light.color.rgb * spot_light.color.a;
                    lighting_data.camera_position = camera_position.xyz;
                    DirectLighting result = integrate_bxdf(data, lighting_data);
                    diffuse += result.diffuse;
                    specular += result.specular;
                }
            }
            break;

    }
    

    float3 color = diffuse + specular;
    float alpha = (color.r + color.g + color.b) / 3.0f;
    if (color.r == 0.f && color.g == 0.f && color.b == 0.f)
    {
        alpha = 0;
    }
    
    return float4(color, alpha);
}
