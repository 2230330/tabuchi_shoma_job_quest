//============================================================
// SH9 integration for one cubemap face
//============================================================

TextureCube<float4> EnvCube : register(t0);
SamplerState LinearClamp : register(s0);

cbuffer SHCB : register(b0)
{
    uint SampleDim; // 例: 32
    uint FaceIndex; // 0..5
    uint2 padding;
};

// 出力：6 faces * 9 coeffs
RWStructuredBuffer<float4> OutSH : register(u0);

static const float PI = 3.14159265359f;

//------------------------------------------------------------
// face + uv -> direction (右手・左手はエンジンに合わせる)
//------------------------------------------------------------
float3 DirFromFaceUV(uint face, float2 uv01)
{
    float2 p = uv01 * 2.0f - 1.0f;

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

//------------------------------------------------------------
// SH9 basis (real SH, Sloan)
//------------------------------------------------------------
void EvalSH9(float3 n, out float sh[9])
{
    float x = n.x;
    float y = n.y;
    float z = n.z;

    sh[0] = 0.282095f;

    sh[1] = 0.488603f * y;
    sh[2] = 0.488603f * z;
    sh[3] = 0.488603f * x;

    sh[4] = 1.092548f * x * y;
    sh[5] = 1.092548f * y * z;
    sh[6] = 0.315392f * (3.0f * z * z - 1.0f);
    sh[7] = 1.092548f * x * z;
    sh[8] = 0.546274f * (x * x - y * y);
}

//立体角
//float TexelSolidAngle(float2 uv01, float invDim)
//{
//    float2 p = uv01 * 2.0f - 1.0f;
//    float x = p.x;
//    float y = p.y;

//    float denom = pow(1.0f + x * x + y * y, 1.5f);
//    float area = (2.0f * invDim) * (2.0f * invDim);

//    return area / denom;
//}
float TexelSolidAngle(float2 uv,float inv_dim)
{
    float2 p = uv * 2 - 1;
    float r2 = 1 + dot(p, p);
    float inv_sqrt = rsqrt(r2);
    float denom = r2 * inv_sqrt;
    float area = (2 * inv_dim) * (2 * inv_dim);
    return area / denom;
}

#define GROUP_X 16
#define GROUP_Y 16

groupshared float3 gAccum[9][GROUP_X * GROUP_Y];

[numthreads(GROUP_X, GROUP_Y, 1)]
void main(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID)
{
    uint dim = SampleDim;
    float invDim = 1.0f / float(dim);

    uint idx = gtid.y * GROUP_X + gtid.x;

    // 初期化（全スレッド必須）
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        gAccum[i][idx] = float3(0, 0, 0);
    }

    bool valid = (gtid.x < dim) && (gtid.y < dim);

    if (valid)
    {
        float2 uv = (float2(gtid.xy) + 0.5f) * invDim;
        float3 dir = DirFromFaceUV(FaceIndex, uv);

        float3 L = EnvCube.SampleLevel(LinearClamp, dir, 2).rgb;
        //明るさのみを取得したいので、各色の最大値を取る
        float luminance = dot(L, float3(0.2126, 0.7152, 0.0722));
        L = luminance.xxx;

        float sh[9];
        EvalSH9(dir, sh);

        float w = TexelSolidAngle(uv, invDim);

        [unroll]
        for (int i = 0; i < 9; ++i)
            gAccum[i][idx] = L * (sh[i] * w);
    }

    GroupMemoryBarrierWithGroupSync();

    // reduce
    for (uint stride = (GROUP_X * GROUP_Y) / 2; stride > 0; stride >>= 1)
    {
        if (idx < stride)
        {
            [unroll]
            for (int i = 0; i < 9; ++i)
                gAccum[i][idx] += gAccum[i][idx + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // face ごとの部分和を書き出す
    if (idx == 0)
    {
        uint base = FaceIndex * 9;

        [unroll]
        for (int i = 0; i < 9; ++i)
        {
            OutSH[base + i] = float4(gAccum[i][0], 0.0f);
        }
    }
}
