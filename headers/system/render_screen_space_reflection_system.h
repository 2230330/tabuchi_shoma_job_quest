#pragma once
//スクリーンスペースリフレクションのシステム
//深度情報と法線情報を元に、画面上で販社の情報を生成します
//反射の情報は、ディファードレンダリングのライティングパスで合成されます。
#include<vector>

#include"i_render_system.h"
#include"../component/component_manager.h"
#include"../framebuffer.h"
#include"../fullscreen_quad.h"
#include"../deferred_g_buffer.h"

class RenderScreenSpaceReflectionSystem :public IRenderSystem
{
public:
    RenderScreenSpaceReflectionSystem(ComponentManager& comp_mng, RenderPass render_pass);

    void Render()override;

    void SetNormalSRV(ID3D11ShaderResourceView* normal_srv) { this->normal_srv_ = normal_srv; }
    void SetDepthSRV(ID3D11ShaderResourceView* depth_srv) { this->depth_srv_ = depth_srv; }
    void SetColorSRV(ID3D11ShaderResourceView* color_srv) { this->color_srv_ = color_srv; }
    void SetParameterSRV(ID3D11ShaderResourceView* parameter_srv) { this->parameter_srv_ = parameter_srv; }
    void SetDepthTex(ID3D11Texture2D* depth_tex) { this->hiz_copy_tex_ = depth_tex; }


    //生成したスクリーンスペースリフレクションのテクスチャを取得する関数
    //このテクスチャは、ディファードレンダリングのライティングパスで合成されます。
    ID3D11ShaderResourceView* GetSSRTexture(){
        return ssr_framebuffer_->GetShaderResourceView(0).Get();
    }
    
private:
    ComponentManager& comp_mng_;

    //スクリーンスペースリフレクションの定数バッファ
    struct SsrConstants
    {
        float distance{ 10.0f };
        int num_steps{ 10 };
        int max_mip{ 6 };
        float thickness{ 0.5f };

        float resolution{ 0.3f };
        float start_bias{ 0.05f };
        float intensity{ 1.0f };
        float dummy;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer>ssr_constant_buffer_ = nullptr;

    //スクリーンスペースリフレクションのシェーダー
    Microsoft::WRL::ComPtr<ID3D11PixelShader>ssr_ps_ = nullptr;
    //SSRにHi-Zを使いたいので、ミップを用意
    const static int MIP_COUNT=6;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>hiz_copy_tex_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> hiz_depth_texture_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>hiz_depth_srv_ = nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>>hiz_uavs_;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>hiz_srvs_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>hiz_process_uavs_[2];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>hiz_process_srvs_[2];
    Microsoft::WRL::ComPtr<ID3D11ComputeShader>hiz_cs_ = nullptr;
    struct HizConstants
    {
        int srv_mip;
        int dst_mip;
        int dummy[2];
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer>hiz_constant_buffer_ = nullptr;


    //スクリーンスペースリフレクションの描画に必要なリソース
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>normal_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>depth_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>color_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>parameter_srv_ = nullptr;
    std::unique_ptr<FrameBuffer>ssr_framebuffer_ = nullptr;
    std::unique_ptr<FullscreenQuad>ssr_fullscreen_quad_ = nullptr;

private:
    //コンピュートシェーダーでHi-Zを行う関数
    void ComputeHiz(ID3D11DeviceContext*context);
};