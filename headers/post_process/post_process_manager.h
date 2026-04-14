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
        return result_srv_;
    }

    void SetEmissiveMap(ID3D11ShaderResourceView* emissive_map) { bloom_->SetEmissiveMap(emissive_map); }

private:
    //画像合成用
    std::unique_ptr<FrameBuffer> synthesiser_framebuffer_;
    std::unique_ptr<FullscreenQuad> fullscreen_transfer_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>synthesiser_ps_;
    
    //最終画像を転送するやつ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> result_srv_;

    //以下、ポストプロセス
    //ブルーム
    std::unique_ptr<Bloom> bloom_;
    Microsoft::WRL::ComPtr<ID3D11BlendState>blend_state_;
    //FXAA
    Microsoft::WRL::ComPtr<ID3D11PixelShader>fxaa_ps_;
    std::unique_ptr<FrameBuffer> fxaa_framebuffer_;

};