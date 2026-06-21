#include"fullscreen_quad.hlsli"

Texture2D<float4> ssr_texture : register(t0);
Texture2D<float4> obj_texture : register(t1);

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
SamplerState sampler_states[6] : register(s0);

float4 main(VS_OUT pin):SV_TARGET
{
    float4 color = { 0.f, 0.f, 0.f, 0.f };
    
    float4 ssr_color = ssr_texture.Sample(sampler_states[POINT_CLAMP], pin.texcoord);
    float4 obj_color = obj_texture.Sample(sampler_states[POINT_CLAMP], pin.texcoord);
    
    color.rgb = (obj_color.rgb * (1.0f - ssr_color.a)) + (ssr_color.rgb * ssr_color.a);
    color.a = obj_color.a;
    
    return color;
}