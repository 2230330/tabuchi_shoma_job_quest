#include "bloom.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4

SamplerState sampler_states[5] : register(s0);

Texture2D scene_texture : register(t0);
Texture2D bloom_texture : register(t1);

// ポストエフェクトの結果を合成するためのシェーダー
float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    float4 scene = scene_texture.Sample(
        sampler_states[LINEAR_CLAMP],
        texcoord
    );

    float3 bloom = bloom_texture.Sample(
        sampler_states[LINEAR_CLAMP],
        texcoord
    ).rgb;

    float3 result = scene.rgb + bloom * bloom_intensity;

    return float4(result, scene.a);
}