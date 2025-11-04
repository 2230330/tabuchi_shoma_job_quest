#pragma once
#include"i_render_system.h"
#include<d3d11.h>
#include<wrl.h>
#include"../component/component_manager.h"

class CloudRenderSystem :public IRenderSystem
{
public:
    CloudRenderSystem(ComponentManager&comp_mng);

    void Render()override;

private:
    ComponentManager& comp_mng_;

    //ドーム頂点構造
    struct SkyVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 texcoord;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer>vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>index_buffer_;

    // ドームメッシュ生成パラメータ
    const unsigned int latitude_segments_ = 32;
    const unsigned int longitude_segments_ = 64;
    const float radius_ = 1000.f;
    UINT index_count_;

    std::vector<SkyVertex> vertices_;
    std::vector<uint32_t> indices_;

    //シェーダー
    Microsoft::WRL::ComPtr<ID3D11VertexShader>cloud_vs_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>cloud_il_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>cloud_ps_;
};