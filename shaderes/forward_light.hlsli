#include"light.hlsli"

cbuffer FORWARD_LIGHT_CONSTANT_BUFFER : register(b3)
{
    uint4 light_count; //	x : 空き, y : ポイントライト数, z : スポットライト数, w : 空き。
    float4 ambient_color;
    float4x4 light_view_projection; //    ライトのビュー射影行列（シャドウマップ用）
    float4x4 inverse_light_view_projection; //    ライトのビュー射影行列の逆行列（シャドウマップ用）
    float2 light_orthographic_size; //ライトの直交投影のサイズ(x:width,y:height)
    float2 light_depth_range; //ライトの直交投影の深度範囲(x:near,y:far)   
    directional_lights directional_light;
    point_lights point_light[8];
    spot_lights spot_light[8];
};