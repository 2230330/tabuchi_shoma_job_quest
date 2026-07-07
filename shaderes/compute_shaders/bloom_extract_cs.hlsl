#include "../bloom.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4

SamplerState sampler_states[5] : register(s0);

Texture2D hdr_color_buffer_texture : register(t0);
Texture2D emissive_color_buffer_texture : register(t1);

RWTexture2D<float4> output_texture : register(u0);

//ブルーム用の輝度抽出
[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;

    if (pixel.x >= output_width || pixel.y >= output_height)
    {
        return;
    }

    float2 uv = (float2(pixel) + 0.5f) / float2(output_width, output_height);

    float3 hdrColor =
        hdr_color_buffer_texture.SampleLevel(
            sampler_states[LINEAR_CLAMP],
            uv,
            0.0f
        ).rgb;

    float3 emissiveColor =
        emissive_color_buffer_texture.SampleLevel(
            sampler_states[LINEAR_CLAMP],
            uv,
            0.0f
        ).rgb;

    hdrColor += emissiveColor * 2.0f;

    float brightness =
        dot(hdrColor.rgb, float3(0.2126f, 0.7152f, 0.0722f));

    float knee =
        bloom_extraction_threshold * bloom_soft_knee + 1e-4f;

    float soft =
        smoothstep(
            bloom_extraction_threshold - knee,
            bloom_extraction_threshold + knee,
            brightness
        );

    float bloomMask =
        saturate((brightness - bloom_extraction_threshold) / knee);

    bloomMask = max(soft, bloomMask);

    float3 result = hdrColor * bloomMask;

    output_texture[pixel] = float4(result, 0.0f);
}