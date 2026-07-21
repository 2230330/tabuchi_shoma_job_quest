#include "fullscreen_quad.hlsli"
#include"deferred_rendering.hlsli"
#include"camera_buffer.hlsli"
#include"light_view_projection.hlsli"
#include"noise_functions.hlsli"
#include "scene_constant_buffer.hlsli"

//簡易的なレイマーチングフォグ

cbuffer FOG_CONSTANT_BUFFER:register(b6)
{
    float fog_steps;
    float fog_max_distance;
    float noise_scale;
    float fog_density;
};

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
#define SHADOWMAP 6
SamplerState sampler_states[7] : register(s0);

Texture2D<float> depth_map : register(t0);
Texture3D<float4> noise_map : register(t1);
static const int shadow_num = 3;
Texture2D shadow_map[shadow_num] : register(t10);

//static const float PI = 3.14159265359f;
static const float time_offset = .10f;

// from: https://www.shadertoy.com/view/4sfgzs credit to iq
float Hash(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

//フェーズ関数(ミー散乱)
float MiePhase(float cos_theta, float g)
{
    //ヘニエイ・グリーンスタイン関数
    float g2 = g * g;
    return (1.0f - g2) / (pow(1.0f + g2 - 2.0f * g * cos_theta, 1.50f) * 4.0f * PI);
}
float SampleFogNoiseAdvanced(float3 world_pos)
{
    
    float3 p = world_pos*noise_scale +   options.z * time_offset;

    float4 n = noise_map.SampleLevel(
        sampler_states[LINEAR_WRAP],
        p,
        0.0
    );

    float perlin_worley = n.r;
    
    float fog_shape = perlin_worley * perlin_worley;

    return saturate(fog_shape);
}
float SampleFogDensity(float3 world_pos)
{
    float base_density = fog_density;

    float noise = SampleFogNoiseAdvanced(world_pos);

    // noise_amount で模様の強さを制御
    float noise_factor = lerp(1.0f, noise, 1.0);

    return base_density * noise_factor;
}

static const float dither_pattern_8x8[64] =
{
    0.0f / 64.0f, 32.0f / 64.0f, 8.0f / 64.0f, 40.0f / 64.0f, 2.0f / 64.0f, 34.0f / 64.0f, 10.0f / 64.0f, 42.0f / 64.0f,
    48.0f / 64.0f, 16.0f / 64.0f, 56.0f / 64.0f, 24.0f / 64.0f, 50.0f / 64.0f, 18.0f / 64.0f, 58.0f / 64.0f, 26.0f / 64.0f,
    12.0f / 64.0f, 44.0f / 64.0f, 4.0f / 64.0f, 36.0f / 64.0f, 14.0f / 64.0f, 46.0f / 64.0f, 6.0f / 64.0f, 38.0f / 64.0f,
    60.0f / 64.0f, 28.0f / 64.0f, 52.0f / 64.0f, 20.0f / 64.0f, 62.0f / 64.0f, 30.0f / 64.0f, 54.0f / 64.0f, 22.0f / 64.0f,
     3.0f / 64.0f, 35.0f / 64.0f, 11.0f / 64.0f, 43.0f / 64.0f, 1.0f / 64.0f, 33.0f / 64.0f, 9.0f / 64.0f, 41.0f / 64.0f,
    51.0f / 64.0f, 19.0f / 64.0f, 59.0f / 64.0f, 27.0f / 64.0f, 49.0f / 64.0f, 17.0f / 64.0f, 57.0f / 64.0f, 25.0f / 64.0f,
    15.0f / 64.0f, 47.0f / 64.0f, 7.0f / 64.0f, 39.0f / 64.0f, 13.0f / 64.0f, 45.0f / 64.0f, 5.0f / 64.0f, 37.0f / 64.0f,
    63.0f / 64.0f, 31.0f / 64.0f, 55.0f / 64.0f, 23.0f / 64.0f, 61.0f / 64.0f, 29.0f / 64.0f, 53.0f / 64.0f, 21.0f / 64.0f
};

float GetDither8x8(float2 pixel_position)
{
    uint2 p = uint2(pixel_position);
    uint x = p.x & 7u;
    uint y = p.y & 7u;

    return dither_pattern_8x8[x + y * 8u];
}


float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = ambient_color;
    
    float4 ndc = float4(2.0 * pin.texcoord.x - 1.0, 1.0 - 2.0 * pin.texcoord.y, 0.0, 1.0);
    float4 pos = mul(ndc, inverse_view_projection_transform);
    pos /= pos.w;
    float3 ray_dir = normalize(pos.xyz-camera_position.xyz);
    
    //オブジェクトの解像度と背景の解像度が別なので、
    //情報の取得場所を計算
    float object_depth = depth_map.Sample(sampler_states[POINT_CLAMP],pin.texcoord.xy);
    
    float3 ray_start = camera_position.xyz ;
    float3 ray_end = camera_position.xyz + (ray_dir * fog_max_distance);
    float3 obj_pos = camera_position.xyz + (ray_dir * fog_max_distance);
    if(object_depth<1.0f)
    {
        obj_pos = camera_position.xyz + (ray_dir * (camera_clip_distance.y * object_depth));
    }
    float camera_to_obj_length = length(obj_pos - ray_start);
    float3 ray_step = (ray_end - ray_start) / fog_steps;
    
    const float4x4 dither_pattern =
    {
        { 0.0f, 0.5f, 0.125f, 0.625f },
        { 0.75f, 0.22f, 0.875f, 0.375f },
        { 0.1875f, 0.6875f, 0.0625f, 0.5625 },
        { 0.9375f, 0.4375f, 0.8125f, 0.3125 }
    };
    //float dither_value = GetDither8x8(pin.position.xy);
    float dither_value = dither_pattern[pin.position.x % 4][pin.position.y % 4];
    float3 ray_current = ray_start + ray_step * dither_value;
    
    float distance=0;
    float step = fog_max_distance / fog_steps;
    float stop_dis = camera_clip_distance.y * object_depth;
    float volume = 0.f;
    bool hit = true;
    
    [loop]
    for (int i = 0; i < fog_steps;i++)
    {
        hit = true;
        if(use_shadow>0)
        {
            //シャドウマップを参照し、陰でなければ加算
            [unroll]
            for (int j = 0; j < shadow_num;j++)
            {
            
                float3 shadow_texcoord;
	            {
		            // ライトから見たNDC座標を算出
                    float4 wvpPos = mul(float4(ray_current, 1.0f), cascade_light_view_projection[j]);
		            // NDC座標からUV座標を算出する
                    wvpPos /= wvpPos.w;
                    wvpPos.y = -wvpPos.y;
                    wvpPos.xy = 0.5f * wvpPos.xy + 0.5f;
                    shadow_texcoord = wvpPos.xyz;
                }
            
                if (shadow_texcoord.z >= 0 && shadow_texcoord.z <= 1 &&
                            shadow_texcoord.x >= 0 && shadow_texcoord.x <= 1 &&
                            shadow_texcoord.y >= 0 && shadow_texcoord.y <= 1)
                {            
                    float depth = shadow_map[j].Sample(sampler_states[SHADOWMAP], shadow_texcoord.xy).r;
                    //深度値を比較して、影じゃなければ加算
                    if (shadow_texcoord.z - depth > shadow_bias)
                    {
                        hit = false;
                        break;
                    }
                }
            }
        }
        
        if(hit)
        {
            volume += SampleFogDensity(ray_current) * length(ray_step);
        }
        
        if (length(ray_current - ray_start) > camera_to_obj_length
            ||volume>=1.0f)
            break;
        else
            ray_current += ray_step;
    }

    
    color = float4(color.rgb*volume, 0);
    return color;
}