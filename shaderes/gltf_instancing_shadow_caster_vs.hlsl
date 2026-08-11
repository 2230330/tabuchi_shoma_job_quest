#include"gltf_model.hlsli"
#include"light_view_projection.hlsli"

VS_OUT main(INSTANCING_VS_IN vin)
{
    float sigma = vin.tangent.w;

    VS_OUT vout;

    float4 local_position = vin.position;
    local_position.w = 1.0f;
    
    //インスタンスごとのスキニング
    if(skin>-1)
    {
        float4x4 skin_matrix = BuildSkinMatrix(vin);
        local_position = mul(local_position, skin_matrix);
    }

    float4x4 instance_world_matrix = BuildInstanceWorldMatrix(vin);
    
    //ノードのワールドとインスタンスのワールドを合成
    float4x4 world_matrix = mul(world, instance_world_matrix);
    
    float4 wpos = mul(local_position, world_matrix);
    
    vout.position = mul(wpos, cascade_light_view_projection[current_index]);
	
    return vout;
}
