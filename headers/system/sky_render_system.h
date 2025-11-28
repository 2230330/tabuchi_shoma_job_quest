#pragma once
#include"i_render_system.h"

#include<d3d11.h>
#include<wrl.h>
#include"../component/component_manager.h"

class SkyRenderSystem:public IRenderSystem
{
public:
    SkyRenderSystem(ComponentManager& comp_mng,RenderPass render_pass);

    void Render()override;
private:
    ComponentManager& comp_mng_;

    UINT frame_count_{0};//マイフレーム計算は重いので、特定のフレームのみで描画する
    

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

        float height{ 0.f };//自身の高度

        DirectX::XMFLOAT3 dummy;
    };
    RayleighConstants rayleigh_constant;
    Microsoft::WRL::ComPtr<ID3D11Buffer>rayleigh_constant_buffer_;
};