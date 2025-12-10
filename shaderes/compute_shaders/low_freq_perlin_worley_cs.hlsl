#include"../perlin_worley_noise.hlsli"

RWTexture3D<float4> low_freq_perlin_worley : register(u0);

#define LOW_FREQ_PERLIN_WORLEY_DIMENSIONS 128
#define LOW_FREQ_PERLIN_WORLEY_NUMTHREADS 8
[numthreads(LOW_FREQ_PERLIN_WORLEY_NUMTHREADS, LOW_FREQ_PERLIN_WORLEY_NUMTHREADS, LOW_FREQ_PERLIN_WORLEY_NUMTHREADS)]
void main(uint3 dtid : SV_DISPATCHTHREADID)
{
    if (dtid.x >= LOW_FREQ_PERLIN_WORLEY_DIMENSIONS ||
        dtid.y >= LOW_FREQ_PERLIN_WORLEY_DIMENSIONS ||
        dtid.z >= LOW_FREQ_PERLIN_WORLEY_DIMENSIONS)
    {
        return;
    }
    
    const float freq = 4.0;
    
    float3 uvw = (float3) (dtid) / LOW_FREQ_PERLIN_WORLEY_DIMENSIONS;
	
    float pfbm = lerp(1.0, perlin_fbm(uvw, freq , 4), 0.5);
    pfbm = abs(pfbm * 2.0 - 1.0); // billowy perlin noise
    
    float4 color = 0;
    color.g = lerp(1.0f,worley_fbm(uvw, freq * 1.0),0.5f);
    color.b = lerp(1.0f,worley_fbm(uvw, freq * 2.0),0.5f);
    color.a = lerp(1.0f, worley_fbm(uvw, freq * 4.0), 0.5f);
    color.r = remap(pfbm, 0.0, 1.0, color.b, 1.0); // perlin-worley

    low_freq_perlin_worley[dtid] =color;
}
