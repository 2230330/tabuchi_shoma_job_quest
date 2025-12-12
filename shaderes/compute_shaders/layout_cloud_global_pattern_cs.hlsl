#include"../perlin_worley_noise.hlsli"
#include"../swirly_curl_noise.hlsli"

RWTexture2D<float4> cloud_global_pattern : register(u0);

float4 MakeSeamless4D(float2 uv)
{
    float tx = uv.x * 6.2831853; // 2ƒÎ
    float ty = uv.y * 6.2831853;

    return float4(
        cos(tx), sin(tx),
        cos(ty), sin(ty)
    );
}


#define CLOUD_GLOBAL_PATTERN_DIMENSIONS 256
#define CLOUD_GLOBAL_PATTERN_NUMTHREADS 8
[numthreads(CLOUD_GLOBAL_PATTERN_NUMTHREADS, CLOUD_GLOBAL_PATTERN_NUMTHREADS, 1)]
void main(uint2 dtid : SV_DISPATCHTHREADID)
{
    if (dtid.x >= CLOUD_GLOBAL_PATTERN_DIMENSIONS ||
        dtid.y >= CLOUD_GLOBAL_PATTERN_DIMENSIONS)
    {
        return;
    }
    
    float2 uv = (float2) (dtid) / CLOUD_GLOBAL_PATTERN_DIMENSIONS;
    float z = 0;
    float3 uvw = float3(uv, z);
	
    float4 color = 0.f;

    color.r = (FlowField(uvw * 16, 1.2f, 15.f) * 0.6) + (FlowField(uvw * 8, 1, 1.2f)*0.4f);
    color.g = FlowField(uvw * 16, 4, 12.f);
    color.b = FlowField(uvw * 32, 8, 12.f);
    color.a = 1.0f - worley_fbm(uvw, 5);

    
    cloud_global_pattern[dtid] = color;
}
