struct VS_INPUT
{
    uint vertexid : SV_VERTEXID;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};