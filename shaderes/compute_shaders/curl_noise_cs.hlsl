#include "../perlin_worley_noise.hlsli"
#include"../swirly_curl_noise.hlsli"

RWTexture3D<float3> CurlNoiseTexture : register(u0);

#define CURL_NOISE_SIZE 128
#define CURL_NOISE_THREAD 8

// 数値微分するときの微小値（小さすぎると精度不安定）
static const float EPS = 0.002;

[numthreads(CURL_NOISE_THREAD, CURL_NOISE_THREAD, CURL_NOISE_THREAD)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= CURL_NOISE_SIZE ||
        dtid.y >= CURL_NOISE_SIZE ||
        dtid.z >= CURL_NOISE_SIZE)
        return;

    float3 uvw = (float3) dtid / CURL_NOISE_SIZE;
    
    float3 p = uvw * 8.0f;
        
    float3 curl = FlowField(p,2.0f,1.2f);

    // 正規化して 1 → 0~1 に収める
    curl = normalize(curl) * 0.5 + 0.5;

    CurlNoiseTexture[dtid] =curl;
}
