#pragma once
#include"i_component.h"
#include<DirectXMath.h>

struct ComponentVolumetricCloud :public IComponent
{
    //雲タイプ別の制御
    DirectX::XMFLOAT4 layout_cloud_type{ 1.0f,1.0f,1.0f,1.0f };
    //レイアウト領域が繰り返される距離の制御
    float layout_cloud_global_scale{ 256 };
    //其々の雲のタイプのグローバルパターンテクスチャのスケール制御
    DirectX::XMFLOAT4 layout_cloud_per_type_scale{ 1.0f,1.0f,1.0f,1.0f };
    //レイアウト領域テクスチャのオフセット及び回転の制御
    DirectX::XMFLOAT4 layout_global_texture_placement{ 0.0f,0.0f,0.0f,0.0f };
    //風の強さを制御,アルファで均一に増加
    DirectX::XMFLOAT4 layout_window_controls{ 0.1f,0.0f,0.1f,0.1f };
    //マスクテクスチャが其々のタイプの雲に及ぼす影響、アルファは全体的な強さ
    DirectX::XMFLOAT4 layout_cloud_type_mask{ 0.0f,0.0f,0.0f,1.f };
    //空を覆う雲の量
    float layout_global_coverage{ -0.2f };
};