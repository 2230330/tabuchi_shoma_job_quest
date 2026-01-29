
// resources/shader/latlong_to_cubemap_cs.hlsl
Texture2D EnvLatLong : register(t0);
SamplerState LinearClamp : register(s0)
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = CLAMP;
    AddressW = WRAP;
};

RWTexture2DArray<float4> SkyCube : register(u0);

static const float PI = 3.14159265359;

float3 DirFromCubeUV(uint face, float2 uv01)
{
    float2 p = uv01 * 2.0 - 1.0;
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

float3 SampleLatLong(float3 dir)
{
    float phi = atan2(dir.z, dir.x);
    float theta = acos(clamp(dir.y,-1.0f,+1.0f));
    float2 uv = float2((phi + PI) / (2.0 * PI), theta / PI);
    return EnvLatLong.SampleLevel(LinearClamp, uv, 0).rgb;
}

[numthreads(4, 4, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint width, height, layers;
    SkyCube.GetDimensions(width, height, layers);

    if (id.x >= width || id.y >= height)
        return;

    float2 uv01 = (float2(id.xy) + 0.5) / float2(width, height);

    [unroll]
    for (uint face = 0; face < 6; ++face)
    {
        float3 dir = DirFromCubeUV(face, uv01);
        float3 rgb = SampleLatLong(dir);
        SkyCube[uint3(id.xy, face)] = float4(rgb, 1.0);
    }
}
