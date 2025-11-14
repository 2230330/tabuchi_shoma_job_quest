#include"cloud_dome.hlsli"
#include"scene_constant_buffer.hlsli"
VS_OUT main(VS_IN vin)
{
    VS_OUT vout;

    // カメラ位置を加算して、スカイドームをカメラの周囲に配置
    float3 dome_pos = vin.position + camera_position.xyz;

    vout.world_pos = dome_pos;
    
    // 通常のビュー・プロジェクション変換（回転は反映させる）
    float4 view_pos = mul(float4(dome_pos, 1.0f), view_transform);
    vout.position = mul(view_pos, projection_transform);
    
    vout.texcoord = vin.texcoord;

    return vout;
}