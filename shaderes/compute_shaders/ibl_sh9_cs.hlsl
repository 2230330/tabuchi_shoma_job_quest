//キューブマップを低解像度でサンプル、9係数へ加算

TextureCube<float4> EnvCube : register(t0);
SamplerState LinearClamp : register(s0);

cbuffer SHCB : register(b0)
{
    uint SampleDim;
    uint FaceIndex;
    uint2 padding;
};

//SetMeshOutputCounts:9*float4 
RWStructuredBuffer<float4> OutSH : register(u0);

static const float PI = 3.14159265359;

//face uv->direction
float3 DirFromFaceUV(uint face, float2 uv01)
{
    float2 p = uv01 * 2.0 - 1.0;

    // 注意：あなたの DirectionFromCubeUV と同じ向きに揃える
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

// SH9 basis (real SH), order 2
void EvalSH9(float3 n, out float sh[9])
{
    // n = (x,y,z)
    float x = n.x;
    float y = n.y;
    float z = n.z;

    // Peter-Pike Sloan の係数
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

// cubemap texel solid angle (approx)
float TexelSolidAngle(float2 uv01, float invDim)
{
    // 近似：面上の立体角重み
    // (x,y) in [-1,1]
    float2 p = uv01 * 2.0 - 1.0;
    float x = p.x;
    float y = p.y;

    float denom = pow(1.0 + x * x + y * y, 1.5);
    // 1 texel area in [-1,1] is (2/dim)^2
    float area = (2.0 * invDim) * (2.0 * invDim);
    return area / denom;
}

#define GROUP_X 16
#define GROUP_Y 16

groupshared float3 gAccum[9][GROUP_X * GROUP_Y];

[numthreads(GROUP_X, GROUP_Y, 1)]
void main(uint3 dtid : SV_DispatchThreadID,
          uint3 gtid : SV_GroupThreadID,
          uint3 gid : SV_GroupID)
{
    uint dim = SampleDim;
    float invDim = 1.0 / float(dim);

    uint idx = gtid.y * GROUP_X + gtid.x;
    
    OutSH[idx] = float4(0, 0, 0, 0);

    // --- 有効スレッドか判定（returnしない！） ---
    bool valid = (gtid.x < dim) && (gtid.y < dim);

    float3 L = float3(0, 0, 0);
    float sh[9];
    [unroll]
    for (int k = 0; k < 9; ++k)
        sh[k] = 0.0;
    float w = 0.0;

    if (valid)
    {
        float2 uv = (float2(gtid.xy) + 0.5) * invDim; // 0..1
        float3 dir = DirFromFaceUV(FaceIndex, uv);

        L = EnvCube.SampleLevel(LinearClamp, dir, 0).rgb;
        EvalSH9(dir, sh);
        w = TexelSolidAngle(uv, invDim);
    }

    // --- sharedへ書く（validでなければ0） ---
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        gAccum[i][idx] = L * (sh[i] * w);
    }

    // --- 全スレッドが必ず到達する barrier ---
    GroupMemoryBarrierWithGroupSync();

    // --- reduce ---
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

    // --- 代表スレッドが書き込み ---
    if (idx == 0)
    {
        uint base = FaceIndex * 9;

        [unroll]
        for (int i = 0; i < 9; ++i)
        {
            OutSH[base + i] = float4(gAccum[i][0], 1.0);
        }
    }
}