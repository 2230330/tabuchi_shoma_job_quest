#pragma once
#include"i_render_system.h"

#include<d3d11.h>
#include<wrl.h>
#include"../component/component_manager.h"

class SkyRenderSystem:public IRenderSystem
{
public:
    SkyRenderSystem(ComponentManager& comp_mng);

    void Render()override;

private:
    ComponentManager& comp_mng_;

    //頂点構造
    struct SkyVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 texcoord;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer>vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>index_buffer_;

    // メッシュ生成パラメータ
    const unsigned int latitude_segments_ = 16;
    const unsigned int longitude_segments_ = 32;
    const float radius_ = 1000.f;
    UINT index_count_;

    std::vector<SkyVertex> vertices_;
    std::vector<uint32_t> indices_;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> sky_vs_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>sky_input_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> sky_ps_;

    //定数バッファ
    // 参考資料
    //https://speakerdeck.com/fadis/konpiyutagurahuikusunokong?slide=26
    struct RayleighConstants
    {
        //DirectX::XMFLOAT4 sun_parameter{1.0f,1.0f,1.0f,20.f};//xyz:color,a:intensity

        float planet_radius{6360000.f};            //地球の半径m
        float atmosphere_radius{6460000.f};        //大気の半径（地球の大気は大体100㎞）m
        float height{ 0.f };//自身の高度

        float rayleigh_scale_height{8.0f};    //b分子密度の減り具合km
        DirectX::XMFLOAT3 rayleigh_coeff{ 0.000005802f,0.000013558f,0.0000331f};//波長依存係数

        float dummy;
    };
    RayleighConstants rayleigh_constant;
    Microsoft::WRL::ComPtr<ID3D11Buffer>rayleigh_constant_buffer_;
};