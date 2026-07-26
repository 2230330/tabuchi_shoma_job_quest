#include"fullscreen_quad.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
#define SHADOWMAP 6
SamplerState sampler_states[7] : register(s0);

Texture2D<float4> back_texture:register(t0);
Texture2D<float4> object_texture:register(t1);
Texture2D<float4> fog_texture:register(t2);

float4 main(VS_OUT pin):SV_TARGET
{
    float3 color;
    float2 uv = pin.texcoord;

    float4 back = back_texture.Sample(sampler_states[LINEAR_CLAMP], uv);
    float4 obj = object_texture.Sample(sampler_states[LINEAR_CLAMP], uv);
    float4 fog = fog_texture.Sample(sampler_states[POINT_CLAMP], uv);
    
    // α合成（オブジェクトが前）
    //float3 color = lerp(back.rgb, obj.rgb, obj.a);
    color.rgb = obj.rgb + (back.rgb * (1.0f - obj.a));
    color.rgb += fog.rgb;
    
    
    return float4(color.rgb, 1.0f);
}