#define USE_LATLONG 0

SamplerState LinearClamp : register(s0);

#if USE_LATLONG
Texture2D EnvLatLong : register(t0);
#else
TextureCube EnvMap : register(t0);
#endif
//前フレームのキューブテクスチャ
TextureCube DiffusePrev : register(t1);

cbuffer DiffuseCB : register(b0)
{
    uint FaceIndex;
    uint FrameIndex;
    float Alpha;
    float MipLOD;
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

void TangentBasis(float3 N, out float3 T, out float3 B)
{
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(0, 1, 0);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

float3 DirectionFromCubeUV(uint face, float2 uv)
{
    float2 p = uv * 2.0 - 1.0;
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

float3 SampleLatLong(float3 dir, float lod)
{
#if USE_LATLONG
    float phi = atan2(dir.z, dir.x);
    float theta = acos(clamp(dir.y,-1.0f,+1.0f));
    float2 uv = float2((phi + PI) / (2.0 * PI), theta / PI);
    return EnvLatLong.SampleLevel(LinearClamp, uv, lod).rgb;
#else
    return 0;
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

// ----------- 拡散用：cosine 重要度サンプリング -----------
float3 CosineSampleHemisphere(float2 u)
{
    float phi = 2.0 * PI * u.x;
    float r = sqrt(u.y);
    float x = r * cos(phi);
    float y = r * sin(phi);
    float z = sqrt(max(0.0, 1.0 - x * x - y * y));
    return float3(x, y, z);
}

// Main
struct PSIn
{
    float4 posH : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    //このピクセルがあらわす方向＝法線N
    float3 N = DirectionFromCubeUV(FaceIndex, i.uv);

    //低周波化とノイズ定位減の為のLOD
    const uint SAMPLE_COUNT = 32; // 16~64
    const float mipLOD = MipLOD; // 低周波化（1~2）
    float3 T, B;
    TangentBasis(N, T, B);

    float3 sum = 0;
    [loop]
    for (uint s = 0; s < SAMPLE_COUNT; ++s)
    {
        //フレーム毎にサンプルを回転
        uint idx = s + FrameIndex * 131u;
        float2 Xi = Hammersley(idx%SAMPLE_COUNT, SAMPLE_COUNT);
        float3 Lh = CosineSampleHemisphere(Xi); // 局所半球
        float3 L = normalize(T * Lh.x + B * Lh.y + N * Lh.z); // 世界方向
        float3 color = SampleEnv(L, mipLOD);
        //輝度抽出
        float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
        sum += luminance.xxx;
        //sum += SampleEnv(L, mipLOD);
    }

    float3 diffuse_new=(sum / SAMPLE_COUNT) * PI;//期待値にπをかける
    
    //前フレームの結果を取得
    float3 diffuse_prev = DiffusePrev.SampleLevel(LinearClamp, N, 0).rgb;
    
    //Temporalブレンド
    float a = saturate(Alpha);
    float3 blended = lerp(diffuse_prev, diffuse_new, a);
    
    return float4(blended, 1.0f);

}
