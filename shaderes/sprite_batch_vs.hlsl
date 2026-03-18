#include "sprite_batch.hlsli"
#include "camera_buffer.hlsli"

VS_OUT main(VS_IN vin)
{
    VS_OUT vout;

    // 頂点座標を float4 に拡張
    float4 pos = float4(vin.position, 1.0f);

    // ワールド → ビュー → プロジェクション
    float4 world_pos = mul(pos, vin.world);
    vout.position = mul(world_pos, view_projection_transform);

    // その他属性
    vout.color = vin.color;
    vout.texcoord = vin.texcoord;

    return vout;
}
