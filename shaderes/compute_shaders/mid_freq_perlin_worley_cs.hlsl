#include"../perlin_worley_noise.hlsli"

RWTexture3D<float4> mid_freq_perlin_worley : register(u0);

#define MID_FREQ_PERLIN_WORLEY_DIMENSIONS 128
#define MID_FREQ_PERLIN_WORLEY_NUMTHREADS 8
[numthreads(MID_FREQ_PERLIN_WORLEY_NUMTHREADS, MID_FREQ_PERLIN_WORLEY_NUMTHREADS, MID_FREQ_PERLIN_WORLEY_NUMTHREADS)]
void main(uint3 dtid : SV_DISPATCHTHREADID)
{
    if (dtid.x >= MID_FREQ_PERLIN_WORLEY_DIMENSIONS ||
        dtid.y >= MID_FREQ_PERLIN_WORLEY_DIMENSIONS ||
        dtid.z >= MID_FREQ_PERLIN_WORLEY_DIMENSIONS)
    {
        return;
    }
    
    const float freq = 8.0;
    
    float3 uvw = (float3) (dtid) / MID_FREQ_PERLIN_WORLEY_DIMENSIONS;
	
    float pfbm = lerp(1.0, perlin_fbm(uvw, freq , 7), 0.8);
    pfbm = abs(pfbm * 2.0 - 1.0); // billowy perlin noise
    
    float4 color = 0;
    color.g = worley_fbm(uvw, freq * 1.0);
    color.b = worley_fbm(uvw, freq * 2.0);
    color.a = worley_fbm(uvw, freq * 4.0);
    color.r = remap(pfbm, 0.0, 1.0, color.g, 1.0); // perlin-worley

    mid_freq_perlin_worley[dtid] = color;
}
