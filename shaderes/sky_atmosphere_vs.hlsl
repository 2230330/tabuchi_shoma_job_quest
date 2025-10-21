#include"sky_atmosphere.hlsli"
#include"scene_constant_buffer.hlsli"

VS_OUT main(VS_IN vin)
{
    VS_OUT vout;
    
    float3 world_pos = vin.position + camera_position.xyz;
    
    float4 view_pos = mul(float4(world_pos, 1.0f), view_transform);
    vout.position = mul(view_pos, projection_transform);
    
    vout.texcoord = vin.texcoord;
    
    return vout;
}