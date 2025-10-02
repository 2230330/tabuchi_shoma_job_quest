#include "geometric_primitive.hlsli"
//VS_OUT main(VS_INPUT input)
//{
//    VS_OUT vout;
//    vout.position = mul(input.position, mul(world, view_projection));
//
//    //input.normal.w = 0;
//    float4 N = normalize(mul(input.normal, world));
//    float4 L = normalize(-light_direction);
//
//    vout.color.rgb = material_color.rgb * max(0, dot(L, N));
//    vout.color.a = material_color.a;
//    return vout;
//}





//instNo : SV_InstanceID 勝手にGPUが割り当ててくれる
//VS_OUT main(VS_INPUT input,uint instNo : SV_InstanceID)
VS_OUT main(float3 position : POSITION,float3 normal : NORMAL,InstanceData instanceData)
{
    VS_OUT vout;

    //ワールド行列で頂点を変換
    float4 world_position = mul(float4(position, 1.0f), mul(instanceData.instance_world,view_projection));

    //ビュー・プロジェクション行列で変換
    //vout.position = mul(world_position, view_projection);
    vout.position = world_position;

    //法線を交換
    vout.normal = normalize(mul(float4(normal, 0.0f), instanceData.instance_world).xyz);
    float4 N = normalize(mul(vout.normal, instanceData.instance_world));
    float4 L = normalize(-light_direction);

    //インスタンスごとの色を適用
    vout.color.rgb = instanceData.color.rgb * max(0, dot(L, N));
    vout.color.a = instanceData.color.a;
    
    return vout;
}