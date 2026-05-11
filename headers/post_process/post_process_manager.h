#pragma once
#include<d3d11.h>
#include<stdint.h>
#include<wrl.h>
#include<memory>

//前方宣言
class FrameBuffer;
class FullscreenQuad;
class Bloom;

    //ポストプロセスを一か所で纏める為のクラス
class PostProcessManager
{
public:
    PostProcessManager(ID3D11Device* device, uint32_t width, uint32_t height);
    ~PostProcessManager();

    void PostProcess(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map);

    void PostImgui();

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetResultShaderResourceView();

    //ブルーム用に自己発光テクスチャを持ってくる関数
    void SetEmissiveMap(ID3D11ShaderResourceView* emissive_map);

private:
    //画像合成用
    std::unique_ptr<FrameBuffer> synthesiser_framebuffer_=nullptr;
    std::unique_ptr<FullscreenQuad> fullscreen_transfer_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>synthesiser_ps_ = nullptr;
    
    //最終画像を転送するやつ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> result_srv_ = nullptr;

    //以下、ポストプロセス
    //ブルーム
    std::unique_ptr<Bloom> bloom_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11BlendState>blend_state_ = nullptr;
    //FXAA
    Microsoft::WRL::ComPtr<ID3D11PixelShader>fxaa_ps_ = nullptr;
    std::unique_ptr<FrameBuffer> fxaa_framebuffer_ = nullptr;

};