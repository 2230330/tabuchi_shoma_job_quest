#include"sprite_batch.hlsli"

Texture2D textures : register(t0);

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
SamplerState sampler_states[5] : register(s0);


float4 main(VS_OUT vin):SV_TARGET
{
    float4 result = textures.Sample(sampler_states[LINEAR_CLAMP], vin.texcoord);
    return result * vin.color;
}