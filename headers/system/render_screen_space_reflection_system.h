#pragma once
//スクリーンスペースリフレクションのシステム
//深度情報と法線情報を元に、画面上で販社の情報を生成します
//反射の情報は、ディファードレンダリングのライティングパスで合成されます。

#include"i_render_system.h"
#include"../component/component_manager.h"
#include"../framebuffer.h"
#include"../fullscreen_quad.h"
#include"../deferred_g_buffer.h"

class ScreenSpaceReflectionRenderSystem :public IRenderSystem
{
public:
    ScreenSpaceReflectionRenderSystem(ComponentManager& comp_mng, RenderPass render_pass);

    void Render()override;

    void SetNormalSRV(ID3D11ShaderResourceView* normal_srv) { this->normal_srv_ = normal_srv; }
    void SetDepthSRV(ID3D11ShaderResourceView* depth_srv) { this->depth_srv_ = depth_srv; }
    void SetColorSRV(ID3D11ShaderResourceView* color_srv) { this->color_srv_ = color_srv; }
    
    void SetSRV(ID3D11ShaderResourceView* srv, int num) { this->srvs_[num] = srv; }


    //生成したスクリーンスペースリフレクションのテクスチャを取得する関数
    //このテクスチャは、ディファードレンダリングのライティングパスで合成されます。
    ID3D11ShaderResourceView* GetSSRTexture(){
        return ssr_framebuffer_->GetShaderResourceView(0).Get();
    }
    
private:
    ComponentManager& comp_mng_;

    //スクリーンスペースリフレクションのシェーダー
    Microsoft::WRL::ComPtr<ID3D11PixelShader>ssr_ps_ = nullptr;
    static constexpr uint8_t kRTCount = static_cast<uint8_t>(Target::Count);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>srvs_[kRTCount];

    //スクリーンスペースリフレクションの描画に必要なリソース
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>normal_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>depth_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>color_srv_ = nullptr;
    std::unique_ptr<FrameBuffer>ssr_framebuffer_ = nullptr;
    std::unique_ptr<FullscreenQuad>ssr_fullscreen_quad_ = nullptr;

};