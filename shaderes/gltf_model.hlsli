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
    
    uint instance_id : SV_InstanceID;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 w_position : TEXCOORD1;
    float4 w_normal : TEXCOORD2;
    float4 w_tangent : TEXCOORD3;
    float2 texcoord : TEXCOORD4;
    float4 current_clip_position : CLIP_POSITION0;
    float4 previous_clip_position : CLIP_POSITION1;
};
#include"camera_buffer.hlsli"

cbuffer PRIMITIVE_CONSTANT_BUFFER : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 previous_world;
    int material;
    int has_tangent;
    int skin;
    int pad;
};

#define HS_IN VS_OUT
#define DS_IN VS_OUT
#define DS_OUT VS_OUT
#define PS_IN VS_OUT

static const uint PRIMITIVE_MAX_JOINTS = 512;
cbuffer PRIMITIVE_JOINT_CONSTANTS : register(b2)
{
    row_major float4x4 joint_matrices[PRIMITIVE_MAX_JOINTS];
};

struct INSTANCE_JOINT_MATRIX
{
    row_major float4x4  value;
};
StructuredBuffer<INSTANCE_JOINT_MATRIX> g_instance_joint_matrices : register(t8);

float4x4 BuildInstanceWorldMatrix(INSTANCING_VS_IN vin)
{

    float4x4 instance_world;
    
    instance_world._11_12_13_14 = vin.world0;
    instance_world._21_22_23_24 = vin.world1;
    instance_world._31_32_33_34 = vin.world2;
    instance_world._41_42_43_44 = vin.world3;
    
    return instance_world;

}

float4x4 BuildPreviousInstanceWorldMatrix(INSTANCING_VS_IN vin)
{

    float4x4 instance_previous_world;
    
    instance_previous_world._11_12_13_14 = vin.previous_world0;
    instance_previous_world._21_22_23_24 = vin.previous_world1;
    instance_previous_world._31_32_33_34 = vin.previous_world2;
    instance_previous_world._41_42_43_44 = vin.previous_world3;
    
    return instance_previous_world;

};

float4x4 GetInstanceJointMatrix(uint instance_id, uint joint_index)
{

    uint matrix_index = instance_id * PRIMITIVE_MAX_JOINTS + joint_index;

    return g_instance_joint_matrices[matrix_index].value;

}

float4x4 BuildSkinMatrix(INSTANCING_VS_IN vin)
{

    float4x4 m0 = GetInstanceJointMatrix(vin.instance_id, vin.joints.x);

    float4x4 m1 = GetInstanceJointMatrix(vin.instance_id, vin.joints.y);

    float4x4 m2 = GetInstanceJointMatrix(vin.instance_id, vin.joints.z);

    float4x4 m3 = GetInstanceJointMatrix(vin.instance_id, vin.joints.w);
    
    return m0 * vin.weights.x +m1 * vin.weights.y +m2 * vin.weights.z +m3 * vin.weights.w;
}