#include "../shaderes/fullscreen_quad.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4

SamplerState sampler_states[5] : register(s0);

// 明示的にスロットを分ける
Texture2D back_texture : register(t0);
Texture2D object_texture : register(t1);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

    float4 back = back_texture.Sample(sampler_states[LINEAR_CLAMP], uv);
    float4 obj = object_texture.Sample(sampler_states[LINEAR_CLAMP], uv);

    // α合成（オブジェクトが前）
    //float3 color = lerp(back.rgb, obj.rgb, obj.a);
    float3 color = obj.rgb + back.rgb * (1.0f - obj.a);

    return float4(color, 1.0f);
}
