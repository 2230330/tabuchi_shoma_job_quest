#ifndef YUZUKI_VOLUMETRIC_CLOUD_HLSLI
#define YUZUKI_VOLUMETRIC_CLOUD_HLSLI

cbuffer CLOUD_PARAMS : register(b12)
{
    //雲タイプ別の制御
    float4 layout_cloud_type;
    //レイアウト領域が繰り返される距離の制御
    float layout_cloud_global_scale;
    float3 patting_0;
    //其々の雲のタイプのグローバルパターンテクスチャのスケール制御
    float4 layout_cloud_per_type_scale;
    //レイアウト領域テクスチャのオフセット及び回転の制御
    float4 layout_global_texture_placement;
    //風の強さを制御,アルファで均一に増加
    float4 layout_window_controls;
    //マスクテクスチャが其々のタイプの雲に及ぼす影響、アルファは全体的な強さ
    float4 layout_cloud_type_mask;
    //空を覆う雲の量
    float layout_global_coverage;
    float3 patting_1;
};

#endif