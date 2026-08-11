#include"gltf_model_gbuffer.hlsli"
#include"camera_buffer.hlsli"


VS_OUT main(INSTANCING_VS_IN vin)
{
    float sigma = vin.tangent.w;

    VS_OUT vout;

    float4 local_position = vin.position;
    local_position.w = 1.0f;

    float4 local_normal = vin.normal;
    local_normal.w = 0.0f;

    float4 local_tangent = vin.tangent;
    local_tangent.w = 0.0f;

    if (skin > -1)
    {
        float4x4 skin_matrix = BuildSkinMatrix(vin);

        local_position = mul(local_position, skin_matrix);
        local_normal = mul(local_normal, skin_matrix);
        local_tangent = mul(local_tangent, skin_matrix);
    }

    float4x4 instance_world_matrix = BuildInstanceWorldMatrix(vin);
    float4x4 instance_previous_world_matrix = BuildPreviousInstanceWorldMatrix(vin);

    float4x4 world_matrix = mul(world, instance_world_matrix);
    float4x4 previous_world_matrix = mul(previous_world, instance_previous_world_matrix);

    float4 wpos = mul(local_position, world_matrix);

    vout.position = mul(wpos, view_projection_transform);
    vout.w_position = wpos;

    vout.w_normal = normalize(mul(local_normal, world_matrix));
    vout.w_tangent = normalize(mul(local_tangent, world_matrix));
    vout.w_tangent.w = sigma;

    vout.texcoord = vin.texcoord;

    vout.current_clip_position = vout.position;
    vout.previous_clip_position =
        mul(mul(local_position, previous_world_matrix), previous_view_projection_transform);

    return vout;
}