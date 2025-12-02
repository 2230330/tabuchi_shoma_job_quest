
#include "../perlin_worley_noise.hlsli"

RWTexture3D<float4> high_freq_worley : register(u0);

#define HIGH_FREQ_WORLEY_DIMENSIONS 32
#define HIGH_FREQ_WORLEY_NUMTHREADS 8

[numthreads(HIGH_FREQ_WORLEY_NUMTHREADS, HIGH_FREQ_WORLEY_NUMTHREADS, HIGH_FREQ_WORLEY_NUMTHREADS)]
void main(uint3 dtid : SV_DISPATCHTHREADID)
{
    if (dtid.x >= HIGH_FREQ_WORLEY_DIMENSIONS ||
        dtid.y >= HIGH_FREQ_WORLEY_DIMENSIONS ||
        dtid.z >= HIGH_FREQ_WORLEY_DIMENSIONS)
    {
        return;
    }

    const float freq = 4.0;
    float3 uvw = (float3) (dtid) / HIGH_FREQ_WORLEY_DIMENSIONS;

    // --- Domain Warp ---
    float3 warp = perlin_fbm(uvw * 0.8, freq,6) * 0.3; // warp strength
    uvw += warp;

    // --- Worley FBM with random rotation ---
    float worley1 = worley_fbm(rotate3D(uvw, 0.5), freq * 1.0);
    float worley2 = worley_fbm(rotate3D(uvw, 1.7), freq * 2.0);
    float worley3 = worley_fbm(rotate3D(uvw, 3.3), freq * 4.0);

    // --- Nonlinear blend ---
    float4 color = 0;
    color.r = pow(worley1, 1.2); // emphasize fine detail
    color.g = worley2;
    color.b = worley3;
    color.a = 1.0;

    high_freq_worley[dtid] = color;
}

