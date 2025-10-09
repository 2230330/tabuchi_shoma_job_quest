#include"../../headers/post_process/post_process_manager.h"
#include"../../external/imgui/imgui.h"

PostProcessManager::PostProcessManager(ID3D11Device* device, uint32_t& width, uint32_t& height, ResourceManager* resource_manager)
    :resource_manager_(resource_manager)
{
    //リザルトを表示する奴
    result_transfer_ = std::make_unique<FullscreenQuad>(device);
    result_synthesiser_ = resource_manager_->LoadPixelShader(device, L".//resources//shader//synthesis_ps.cso");

    bloom_ = std::make_unique<Bloom>(device, width, height,resource_manager_);
}

void PostProcessManager::PostProcess(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map)
{
    //ブルーム処理
    bloom_->Make(immediate_context, color_map);
    ID3D11ShaderResourceView* srv[]
    {
        color_map,
        bloom_->GetShaderResourceView().Get()
    };
    result_transfer_->blit(immediate_context, srv, 0, _countof(srv), result_synthesiser_.Get());
}

void PostProcessManager::PostImgui()
{
    ImGui::Begin("PostProcess");
    bloom_->DrawImgui();

    ImGui::End();
}
