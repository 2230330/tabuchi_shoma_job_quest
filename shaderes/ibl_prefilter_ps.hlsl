// ======================================
// ibl_prefilter_ps.hlsl
// Specular prefilter for PBR IBL (GGX convolution)
// Input: LatLong (2D) or TextureCube
// ======================================

#define USE_LATLONG 0 // 1: LatLong, 0: Cubemap

SamplerState LinearClamp : register(s0);

#if USE_LATLONG
Texture2D EnvLatLong : register(t0);
#else
TextureCube EnvMap : register(t0);
#endif

cbuffer PrefilterCB : register(b0)
{
    uint FaceIndex;
    float Roughness;
    float MipCount; // C++ 側 mip count を使うなら設定
}

static const float PI = 3.14159265359;

// Van der Corput radical inverse (base 2)
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    return float(bits) * 2.3283064365386963e-10; // / 2^32
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

// ウェイビングするための接線基底
void TangentBasis(float3 N, out float3 T, out float3 B)
{
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

// GGX (Trowbridge-Reitz) importance sample
float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 N)
{
    float a = roughness * roughness;
    float a2 = a * a;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y));
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));

    float3 Ht = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    float3 T, B;
    TangentBasis(N, T, B);
    float3 H = normalize(Ht.x * T + Ht.y * B + Ht.z * N);
    return H;
}

// ---- Convert face UV to cubemap direction ----
float3 DirectionFromCubeUV(uint face, float2 uv)
{
    float2 p = uv * 2.0 - 1.0; // 0..1 → -1..1
    if (face == 0)
        return normalize(float3(+1, -p.y, -p.x));
    if (face == 1)
        return normalize(float3(-1, -p.y, +p.x));
    if (face == 2)
        return normalize(float3(p.x, +1, p.y));
    if (face == 3)
        return normalize(float3(p.x, -1, -p.y));
    if (face == 4)
        return normalize(float3(p.x, -p.y, +1));
    return normalize(float3(-p.x, -p.y, -1));
}

// ---- Sampling for LatLong maps ----
float3 SampleLatLong(float3 dir, float lod)
{
    float phi = atan2(dir.z, dir.x);
    float theta = acos(clamp(dir.y,-1.0f,+1.0f));
    float2 uv = float2((phi + PI) / (2.0 * PI), theta / PI);
#if USE_LATLONG
    return EnvLatLong.SampleLevel(LinearClamp, uv, lod).rgb;
#else
    return float3(0.0, 0.0, 0.0);
#endif
}

float3 SampleEnv(float3 dir, float lod)
{
#if USE_LATLONG
    return SampleLatLong(dir, lod);
#else
    return EnvMap.SampleLevel(LinearClamp, dir, lod).rgb;
#endif
}

// -------------------------
// Main
// -------------------------
struct PSIn
{
    float4 posH : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    float3 N = DirectionFromCubeUV(FaceIndex, i.uv);
    float3 V = N;

    const uint BASE = 256;
    uint SAMPLE_COUNT = max(32u, BASE >> (uint) (Roughness * 5.0));

    float3 accum = float3(0.0, 0.0, 0.0);
    float weight = 0.0;

    for (uint s = 0; s < SAMPLE_COUNT; ++s)
    {
        float2 Xi = Hammersley(s, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, Roughness, N);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = saturate(dot(N, L));
        if (NdotL > 0.0)
        {
            // mip の概算：roughness に比例 (MipCount を使うなら Roughness * (MipCount-1) 等に調整)
            float lod = Roughness * (MipCount-1.0f); // C++ 側 mipCount に合わせて必要であれば変更
            float3 c = SampleEnv(L, lod);

            accum += c * NdotL;
            weight += NdotL;
        }
    }

    float3 result = accum / max(weight, 1e-4);
    return float4(result, 1.0);
}