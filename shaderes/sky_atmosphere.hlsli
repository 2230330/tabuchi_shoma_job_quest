struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

struct VS_IN
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};