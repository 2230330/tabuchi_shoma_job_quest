#ifndef __SCENE_CONSTANT_BUFFER_H__
#define __SCENE_CONSTANT_BUFFER_H__

cbuffer SCENE_CONSTANT_BUFFER : register(b1)
{
    float4 options; //	xy : マウスの座標値, z : タイマー, w : フラグ
    float4 z_buffer_parameteres; // 非線形深度から線形深度へ変換するためのパラメーター
    float4 viewport_size; //  xy : ビューポートサイズ, zw : 逆ビューポートサイズ
    float2 sun_uv;
    float2 padding0;
};

#endif  //  __SCENE_CONSTANT_BUFFER_H__
