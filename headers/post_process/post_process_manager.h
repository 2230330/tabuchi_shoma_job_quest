#pragma once
#include<d3d11.h>
#include<stdint.h>
#include<wrl.h>
#include<memory>

#include"../resource_manager.h"
#include"bloom.h"
#include"../fullscreen_quad.h"
#include"../framebuffer.h"


    //ポストプロセスを一か所で纏める為のクラス
class PostProcessManager
{
public:
    PostProcessManager(ID3D11Device* device, uint32_t width, uint32_t height);

    void PostProcess(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map);

    void PostImgui();

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetResultShaderResourceView()
    {
        return result_framebuffer_->GetShaderResourceView(0);
    }

    void SetEmissiveMap(ID3D11ShaderResourceView* emissive_map) { bloom_->SetEmissiveMap(emissive_map); }

private:
    //最終画像を転送するやつ
    std::unique_ptr<FrameBuffer> result_framebuffer_;
    std::unique_ptr<FullscreenQuad> result_transfer_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>result_synthesiser_ps_;

    //以下、ポストプロセス
    std::unique_ptr<Bloom> bloom_;
    Microsoft::WRL::ComPtr<ID3D11BlendState>blend_state_;

};