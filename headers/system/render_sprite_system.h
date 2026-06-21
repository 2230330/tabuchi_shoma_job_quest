#pragma once
#include"i_render_system.h"

#include<d3d11.h>
#include<wrl.h>
#include<DirectXMath.h>
#include<unordered_map>
#include<vector>

//前方宣言
class ComponentManager;

//2?テクスチャの描画
class SpriteRenderSystem :public IRenderSystem
{
public:
    SpriteRenderSystem(ComponentManager&comp_mng,RenderPass render_pass);
    ~SpriteRenderSystem();

private:
    void Render()override;

    //バッチの情報を取得
    struct BatchBufferInfo
    {
        Microsoft::WRL::ComPtr<ID3D11Buffer>buffer;
        size_t cepacity;
    };

    struct VertexBuffer
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 texcoord;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer>vertex_buffer_;

    std::unordered_map <ID3D11ShaderResourceView*, BatchBufferInfo > instance_buffer_pool_;
    ComponentManager& comp_mng_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>default_ps_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>default_vs_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>default_input_ = nullptr;
};