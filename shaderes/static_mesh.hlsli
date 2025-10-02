struct VS_INPUT
{
    float4 position : POSITION;
    float4 normal   : NORMAL;
    float2 texcoord : TEXCOORD;
    row_major float4x4 instance_world : INSTANCE_WORLD;
    float4 instance_color             : INSTANCE_COLOR;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    //float4 normal : NORMAL;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
    float4 world_position : POSITION ;
    float4 world_normal :NORMAL;
};

cbuffer SCENE_CONSTRACT_BUFFER : register(b1)
{
    row_major float4x4 view_projection;
    float4 light_direction;
    float4 camera_position;
};