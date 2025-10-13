struct VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord : TEXCOORD;
    uint4 joints : JOINTS;
    float4 weights : WEIGHTS;
};

struct INSTANCING_VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord : TEXCOORD;
    uint4 joints : JOINTS;
    float4 weights : WEIGHTS;
    float4 world0 : WORLD_MATRIX0;
    float4 world1 : WORLD_MATRIX1;
    float4 world2 : WORLD_MATRIX2;
    float4 world3 : WORLD_MATRIX3;
    float4 previous_world0 : PREVIOUS_WORLD_MATRIX0;
    float4 previous_world1 : PREVIOUS_WORLD_MATRIX1;
    float4 previous_world2 : PREVIOUS_WORLD_MATRIX2;
    float4 previous_world3 : PREVIOUS_WORLD_MATRIX3;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 w_position : POSITION;
    float4 w_normal : NORMAL;
    float4 w_tangent : TANGENT;
    float2 texcoord : TEXCOORD;
};
#include"scene_constant_buffer.hlsli"

cbuffer PRIMITIVE_CONSTANT_BUFFER : register(b0)
{
    row_major float4x4 world;
    int material;
    bool has_tangent;
    int skin;
    int pad;
};

//cbuffer SCENE_CONSTANT_BUFFER : register(b1)
//{
//    row_major float4x4 view_projection;
//    float4 light_direction;
//    float4 camera_position;
//    float4 light_color;
//    float light_intensity;
//    float3 dummy;
//};

static const uint PRIMITIVE_MAX_JOINTS = 512;
cbuffer PRIMITIVE_JOINT_CONSTANTS : register(b3)
{
    row_major float4x4 joint_matrices[PRIMITIVE_MAX_JOINTS];
};
