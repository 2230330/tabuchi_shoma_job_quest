#include"sky_atmosphere.hlsli"

Texture2D sky_texture : register(t0);

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
SamplerState sampler_states[5] : register(s0);

float4 main(VS_OUT vout):SV_TARGET
{
    return sky_texture.Sample(sampler_states[LINEAR_CLAMP], vout.texcoord);
}