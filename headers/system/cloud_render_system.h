#pragma once
#include"i_render_system.h"
#include<d3d11.h>
#include<wrl.h>
#include"../component/component_manager.h"
#include"../fullscreen_quad.h"

class CloudRenderSystem :public IRenderSystem
{
public:
    CloudRenderSystem(ComponentManager&comp_mng);

    void Render()override;

private:
    void UpdateConstants(const ComponentCloudDome& cloud);

    ComponentManager& comp_mng_;

    //ドーム頂点構造
    struct SkyVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 texcoord;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer>vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>index_buffer_;

    // ドームメッシュ生成パラメータ
    const float radius_ = 500.f;
    UINT index_count_;

    std::vector<SkyVertex> vertices_;
    std::vector<uint32_t> indices_;

    //シェーダー
    Microsoft::WRL::ComPtr<ID3D11VertexShader>cloud_vs_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>cloud_il_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>cloud_ps_;


    //定数バッファ
    struct CloudRayMarchingConstants
    {
        int iteration{ 32 };//雲が存在する限界点
        float intensity{1.0f};//雲の輝度、シャフト強度
        float fog_scale{0.01f}; //   雲のスケール倍率
        float step_size{ 100.f };//マーチング一ステップの長さ

        float max_distance{ 5000.f };//雲を追跡する最大距離
        float noise_intensity{ 1.5f };//ノイズのコントラスト
        float noise_threshold{ 0.1f };//雲の密度閾値
        float noise_seed{ 0.0f };//雲の乱数オフセット(風邪移動など)

        float alpha_scale{ 1.0f };//雲全体の透過率補正
        float light_scatter_strength{ 1.f };//光の散乱補正
        float base_brightness{ 1.f };//雲の基本輝度
        float padding0{ 0 };

        DirectX::XMFLOAT3 wind_direction{ 0.01f,0.f,0.f };//雲を動かす方向
        float padding1{ 0 };

        float cloud_base{ 2000.f };//雲の底面
        float cloud_top{ 5000.f };//雲の上面
        DirectX::XMFLOAT2 padding2{ 0,0 };

    };
    CloudRayMarchingConstants cloud_ray_marching_constant_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>cloud_ray_marching_constant_buffer_;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>low_res_srv = nullptr;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>low_res_rtv=nullptr;
    D3D11_VIEWPORT vp{};
    float down_sample{ 1.f };
    std::unique_ptr<FullscreenQuad>fullscreen_quad_;
};