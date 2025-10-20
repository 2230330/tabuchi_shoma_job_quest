#pragma once
#include"i_render_system.h"

#include<DirectXMath.h>
#include<unordered_map>
#include<vector>

#include"../component/component_manager.h"

class SpriteRenderSystem :public IRenderSystem
{
public:
    SpriteRenderSystem(ComponentManager&comp_mng);
    ~SpriteRenderSystem();

private:
    void Render()override;

    //ƒoƒbƒ`‚Ìî•ñ‚ğæ“¾
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
    Microsoft::WRL::ComPtr<ID3D11PixelShader>default_ps_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>default_vs_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>default_input_;
};