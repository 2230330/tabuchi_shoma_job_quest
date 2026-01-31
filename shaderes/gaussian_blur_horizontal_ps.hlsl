// BLOOM
#include"bloom.hlsli"
#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
SamplerState sampler_states[5] : register(s0);

Texture2D hdr_color_buffer_texture : register(t0);

//横方向の暈し
float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    uint mip_level = 0, width, height, number_of_levels;
    hdr_color_buffer_texture.GetDimensions(mip_level, width, height, number_of_levels);
    const float aspect_ratio = width / height;

	//http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
    const float offset[3] = { 0.0, 1.3846153846, 3.2307692308 };
    const float weight[3] = { 0.2270270270, 0.3162162162, 0.0702702703 };

    float4 sampled_color = hdr_color_buffer_texture.Sample(sampler_states[LINEAR_CLAMP], texcoord) * weight[0];
    for (int i = 1; i < 3; i++)
    {
        //半径の追加
        float scale_offset = offset[i] * bloom_radius;
        
        sampled_color += hdr_color_buffer_texture.Sample(sampler_states[LINEAR_CLAMP], texcoord + float2(scale_offset / width, 0.0)) * weight[i];
        sampled_color += hdr_color_buffer_texture.Sample(sampler_states[LINEAR_CLAMP], texcoord - float2(scale_offset / width, 0.0)) * weight[i];
    }
    return sampled_color;
}
