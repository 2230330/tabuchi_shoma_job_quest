//struct VS_INPUT
//{
//    float3 position : POSITION;
//    float3 normal : NORMAL;
//    //float4x4 instance_transform: INSTANCE_TRANSFORM;
//    //float4 instance_color : INSTANCE_COLOR;
//};
struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
};
struct InstanceData
{
    row_major float4x4 instance_world : INSTANCE_WORLD;
    float4   color : INSTANCE_COLOR;
};

//cbuffer OBJECT_CONSTANT_BUFFER : register(b0)
//{
//    row_major float4x4 world;
//    float4 material_color;
//};
cbuffer SCENE_CONSTANT_BUFFER : register(b1)
{
    row_major float4x4 view_projection;
    float4 light_direction;
};