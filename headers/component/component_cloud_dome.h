#pragma once
#include"i_component.h"

struct ComponentCloudDome :public IComponent
{
    int iteration{ 128 };//雲が存在する限界点
    float intensity{ 1.0f };//雲の輝度、シャフト強度
    float fog_scale{ 0.01f }; //   雲のスケール倍率
    float step_size{ 10.f };//マーチング一ステップの長さ

    float max_distance{ 10000.f };//雲を追跡する最大距離
    float noise_intensity{ 1.5f };//ノイズのコントラスト
    float noise_threshold{ 0.1f };//雲の密度閾値
    float noise_seed{ 0.0f };//雲の乱数オフセット(風邪移動など)

    float alpha_scale{ 1.0f };//雲全体の透過率補正
    float light_scatter_strength{ 1.f };//光の散乱補正
    float base_brightness{ 1.f };//雲の基本輝度
    DirectX::XMFLOAT3 wind_direction{ 0.01f,0.f,0.f };//雲を動かす方向

    float cloud_base{ 500.f };//雲の底面
    float cloud_top{ 2000.f };//雲の上面
};