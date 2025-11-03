struct VS_IN
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 world_pos : TEXCOORD1;
};


cbuffer RAYLEIGH_CONSTANT_BUFFER_ : register(b10)
{
    //float4 sun_parameter; //xyz:color,a:intensity
    

    float height;//é©êgÇÃçÇìx
    float3 dummy;

};