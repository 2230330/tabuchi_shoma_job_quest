#include "../bloom.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4

SamplerState sampler_states[5] : register(s0);

Texture2D input_texture : register(t0);
RWTexture2D<float4> output_texture : register(u0);

//ブルーム用のダウンサンプリング  
[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;

    if (pixel.x >= output_width || pixel.y >= output_height)
    {
        return;
    }

    float2 uv = (float2(pixel) + 0.5f) / float2(output_width, output_height);
    float2 texel = 1.0f / float2(input_width, input_height);

    float radius = max(bloom_radius, 0.25f);

    float4 color = 0.0f;

    color += input_texture.SampleLevel(
        sampler_states[LINEAR_CLAMP],
        uv + texel * float2(-0.5f, -0.5f) * radius,
        0.0f
    );

    color += input_texture.SampleLevel(
        sampler_states[LINEAR_CLAMP],
        uv + texel * float2(0.5f, -0.5f) * radius,
        0.0f
    );

    color += input_texture.SampleLevel(
        sampler_states[LINEAR_CLAMP],
        uv + texel * float2(-0.5f, 0.5f) * radius,
        0.0f
    );

    color += input_texture.SampleLevel(
        sampler_states[LINEAR_CLAMP],
        uv + texel * float2(0.5f, 0.5f) * radius,
        0.0f
    );

    output_texture[pixel] = color * 0.25f;
}