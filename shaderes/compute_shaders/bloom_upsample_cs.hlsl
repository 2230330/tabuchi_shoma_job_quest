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

    //float4 lower =
    //    lower_texture.SampleLevel(
    //        sampler_states[LINEAR_CLAMP],
    //        uv,
    //        0.0f
    //    );

    //float lower_weight = 0.65f;

    //float4 result = current + lower * lower_weight;
    
    float2 texel =
    1.0f / float2(output_width, output_height);

    float4 s0 = lower_texture.SampleLevel(sampler_states[LINEAR_CLAMP], uv + texel * float2(-1, -1), 0);
    float4 s1 = lower_texture.SampleLevel(sampler_states[LINEAR_CLAMP], uv + texel * float2(0, -1), 0);
    float4 s2 = lower_texture.SampleLevel(sampler_states[LINEAR_CLAMP], uv + texel * float2(1, -1), 0);

    float4 s3 = lower_texture.SampleLevel(sampler_states[LINEAR_CLAMP], uv + texel * float2(-1, 0), 0);
    float4 s4 = lower_texture.SampleLevel(sampler_states[LINEAR_CLAMP], uv, 0);
    float4 s5 = lower_texture.SampleLevel(sampler_states[LINEAR_CLAMP], uv + texel * float2(1, 0), 0);

    float4 s6 = lower_texture.SampleLevel(sampler_states[LINEAR_CLAMP], uv + texel * float2(-1, 1), 0);
    float4 s7 = lower_texture.SampleLevel(sampler_states[LINEAR_CLAMP], uv + texel * float2(0, 1), 0);
    float4 s8 = lower_texture.SampleLevel(sampler_states[LINEAR_CLAMP], uv + texel * float2(1, 1), 0);

    float4 filtered =
    (
    s0 + s2 + s6 + s8 +
    2.0f * (s1 + s3 + s5 + s7) +
    4.0f * s4
    ) / 16.0f;

    float4 result =
    current +
    filtered * 1.f;

    output_texture[pixel] = result;
}