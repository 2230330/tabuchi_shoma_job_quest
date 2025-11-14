//BLOOM
#include "bloom.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
SamplerState sampler_states[5] : register(s0);

static const uint downsampled_count = 6;
Texture2D downsampled_textures[downsampled_count] : register(t0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    float3 sampled_color = 0;
    float total_weight = 0;

    // ダウンサンプルごとに強度を減らす
    [unroll]
    for (uint i = 0; i < downsampled_count; ++i)
    {
        float weight = 1.0 / pow(2.0, i); // 各レベルの寄与を減らす
        sampled_color += downsampled_textures[i].Sample(sampler_states[LINEAR_CLAMP], texcoord).rgb * weight;
        total_weight += weight;
    }

    sampled_color /= total_weight; // 平均化
    sampled_color *= bloom_intensity; // 定数で全体強度を制御

    float alpha = 1.0f;
    
    return float4(sampled_color, alpha);
}
