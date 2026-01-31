
#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
SamplerState sampler_states[5] : register(s0);

Texture2D hdr_color_buffer_texture : register(t0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    return hdr_color_buffer_texture.SampleLevel(sampler_states[LINEAR_CLAMP], texcoord, 0.0);
}
