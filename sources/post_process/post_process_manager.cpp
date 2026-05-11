#include"../../headers/post_process/post_process_manager.h"
#include"../../headers/graphics.h"
#include"../../headers/render_state.h"
#include"../../headers/resource_manager.h"
#include"../../headers/post_process/bloom.h"
#include"../../headers/fullscreen_quad.h"
#include"../../headers/framebuffer.h"

#include"../../external/imgui/imgui.h"

PostProcessManager::PostProcessManager(ID3D11Device* device, uint32_t width, uint32_t height)
{
    //リザルトを表示する奴
    synthesiser_framebuffer_ = std::make_unique<FrameBuffer>(device, width, height);
    fullscreen_transfer_ = std::make_unique<FullscreenQuad>(device);
    synthesiser_ps_ = ResourceManager::Instance().LoadPixelShader(device, L".//resources//shader//synthesis_ps.cso");

    //ブルーム
    bloom_ = std::make_unique<Bloom>(device, width, height);
    RenderState render_state(device);
    blend_state_ = render_state.GetBlendState(BlendState::transparency);

    //FXAA
    fxaa_ps_ =
        ResourceManager::Instance().LoadPixelShader(device,
            L".\\resources\\shader\\fast_approximate_anti_aliasing_ps.cso");
    fxaa_framebuffer_ = std::make_unique<FrameBuffer>(device, width, height);
}

PostProcessManager::~PostProcessManager() = default;

void PostProcessManager::PostProcess(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map)
{
    //最初の設定を覚えておく
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  cached_depth_stencil_state;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>  cached_rasterizer_state;
    Microsoft::WRL::ComPtr<ID3D11BlendState>  cached_blend_state;
    FLOAT blend_factor[4];
    UINT sample_mask;
    immediate_context->OMGetDepthStencilState(cached_depth_stencil_state.GetAddressOf(), 0);
    immediate_context->RSGetState(cached_rasterizer_state.GetAddressOf());
    immediate_context->OMGetBlendState(cached_blend_state.GetAddressOf(), blend_factor, &sample_mask);

    //ポストエフェクト
    {

        //ブルーム処理
        bloom_->Make(immediate_context, color_map);

        //ポストエフェクトの合成
        synthesiser_framebuffer_->Clear(immediate_context);
        synthesiser_framebuffer_->Activate(immediate_context);
        {
            ID3D11ShaderResourceView* srv[]
            {
                color_map,
                bloom_->GetShaderResourceView().Get()
            };
            immediate_context->OMSetBlendState(blend_state_.Get(), nullptr, 0XFFFFFFFF);
            fullscreen_transfer_->blit(immediate_context, srv, 0, _countof(srv), synthesiser_ps_.Get());
        }
        synthesiser_framebuffer_->Deactivate(immediate_context);

        //FXAA処理
        fxaa_framebuffer_->Clear(immediate_context);
        fxaa_framebuffer_->Activate(immediate_context);
        {
            ID3D11ShaderResourceView* fxaa_srvs[]
            {
                synthesiser_framebuffer_->GetShaderResourceView(0).Get(),
            };
            fullscreen_transfer_->blit(immediate_context, fxaa_srvs, 0, 1, fxaa_ps_.Get());
        

        }
        fxaa_framebuffer_->Deactivate(immediate_context);
    }
    result_srv_ = fxaa_framebuffer_->GetShaderResourceView(0);


    //元の設定に戻す
    immediate_context->OMSetDepthStencilState(cached_depth_stencil_state.Get(), 0);
    immediate_context->RSSetState(cached_rasterizer_state.Get());
    immediate_context->OMSetBlendState(cached_blend_state.Get(), blend_factor, sample_mask);

}

void PostProcessManager::PostImgui()
{
    ImGui::Begin("PostProcess");
    bloom_->DrawImgui();

    ImGui::End();
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> PostProcessManager::GetResultShaderResourceView()
{
    return result_srv_;
}

void PostProcessManager::SetEmissiveMap(ID3D11ShaderResourceView* emissive_map)
{
    bloom_->SetEmissiveMap(emissive_map);
}
