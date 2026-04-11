#include"../../headers/post_process/post_process_manager.h"
#include"../../headers/graphics.h"
#include"../../headers/render_state.h"
#include"../../external/imgui/imgui.h"

PostProcessManager::PostProcessManager(ID3D11Device* device, uint32_t width, uint32_t height)
{
    //リザルトを表示する奴
    result_framebuffer_ = std::make_unique<FrameBuffer>(device, width, height);
    result_transfer_ = std::make_unique<FullscreenQuad>(device);
    result_synthesiser_ps_ = ResourceManager::Instance().LoadPixelShader(device, L".//resources//shader//synthesis_ps.cso");

    bloom_ = std::make_unique<Bloom>(device, width, height);
    RenderState render_state(device);
    blend_state_ = render_state.GetBlendState(BlendState::transparency);
}

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
        result_framebuffer_->Clear(immediate_context);
        result_framebuffer_->Activate(immediate_context);
        ID3D11ShaderResourceView* srv[]
        {
            color_map,
            bloom_->GetShaderResourceView().Get()
        };
        immediate_context->OMSetBlendState(blend_state_.Get(), nullptr, 0XFFFFFFFF);
        result_transfer_->blit(immediate_context, srv, 0, _countof(srv), result_synthesiser_ps_.Get());

        result_framebuffer_->Deactivate(immediate_context);

    }

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
