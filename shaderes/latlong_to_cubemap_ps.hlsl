//latlong->cubemap pixel shader

Texture2D EnvLatLong : register(t0);

SamplerState LinearClamp : register(s0)
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Clamp;
    AddressW = Wrap;
};


static const float PI = 3.14159265359;

float3 DirFromCubeUV(uint face,float2 uv01)
{
    float2 p = uv01 * 2.0f - 1.0f;
    if (face == 0)
        return normalize(float3(+1.0f, -p.y, -p.x));
    if(face == 1)
        return normalize(float3(-1.0f, -p.y, +p.x));
    if(face==2)
        return normalize(float3(p.x, +1.0f, p.y));
    if(face==3)
        return normalize(float3(p.x, -1.0f, -p.y));
    if(face==4)
        return normalize(float3(p.x, -p.y, +1.0f));
    
    return normalize(float3(-p.x, -p.y, -1.0f));

}

float3 SampleLatLong(float3 dir)
{
    float phi = atan2(dir.z, dir.x);
    float theta = acos(clamp(dir.y,-1.0f,+1.0f));
    float2 uv = float2((phi + PI)/(2.0f * PI), theta / PI);
    return EnvLatLong.SampleLevel(LinearClamp, uv,0).rgb;
}

struct PSIn
{
    float4 pos: SV_POSITION;
    float2 uv : TEXCOORD0;
    uint face : SV_RenderTargetArrayIndex;
};

float4 main(PSIn input) : SV_Target
{
    float3 dir = DirFromCubeUV(input.face, input.uv);
    float3 color = SampleLatLong(dir);
    return float4(color, 1.0f);
}