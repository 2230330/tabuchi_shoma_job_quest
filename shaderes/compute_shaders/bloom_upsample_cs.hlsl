#include "../bloom.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4

SamplerState sampler_states[5] : register(s0);

Texture2D current_texture : register(t0);
Texture2D lower_texture : register(t1);

RWTexture2D<float4> output_texture : register(u0);

//ブルーム用のアップサンプリング
[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;

    if (pixel.x >= output_width || pixel.y >= output_height)
    {
        return;
    }

    float2 uv = (float2(pixel) + 0.5f) / float2(output_width, output_height);

    float4 current =
        current_texture.SampleLevel(
            sampler_states[LINEAR_CLAMP],
            uv,
            0.0f
        );

    float4 lower =
        lower_texture.SampleLevel(
            sampler_states[LINEAR_CLAMP],
            uv,
            0.0f
        );

    float lower_weight = 0.65f;

    float4 result = current + lower * lower_weight;

    output_texture[pixel] = result;
}