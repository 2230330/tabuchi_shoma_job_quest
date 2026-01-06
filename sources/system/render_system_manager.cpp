#include"../../headers/system/render_system_manager.h"

#include"../../headers/graphics.h"
#include"../../headers/render_state.h"
#include"../../headers/system/render_system.h"
#include"../../headers/system/instancing_render_system.h"
#include"../../headers/system/sprite_render_system.h"
#include"../../headers/system/sky_render_system.h"
#include"../../headers/system/cloud_render_system.h"


RenderSystemManager::RenderSystemManager(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
    AddSystem(std::make_unique<SkyRenderSystem>(comp_mng_,RenderPass_Background));
    AddSystem(std::make_unique<CloudRenderSystem>(comp_mng_,RenderPass_Background));
    AddSystem(std::make_unique<GltfRenderSystem>(comp_mng_,RenderPass_Object));
    AddSystem(std::make_unique<InstancingRenderSystem>(comp_mng_,RenderPass_Object));
    AddSystem(std::make_unique<SpriteRenderSystem>(comp_mng_,RenderPass_Object));

    bit_block_transfer_ = std::make_unique<FullscreenQuad>(Graphics::Instance().GetDevice());
    back_framebuffer_ = std::make_unique<FrameBuffer>(
        Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()/back_scale),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight()/back_scale)//背景は小さく描画、アップサンプリングで合成
    );
    object_framebuffer_ = std::make_unique<FrameBuffer>(
        Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight())
    );
}

void RenderSystemManager::AddSystem(std::unique_ptr<IRenderSystem> system)
{
    systems_.emplace_back(std::move(system));
}

void RenderSystemManager::RenderAll()
{
    //背景用フレームバッファ
    if (back_sample_count_ < back_sample_rimit_)
    {
        back_sample_count_++;
    }
    else
    {
        back_sample_count_ = 0;
        back_framebuffer_->Clear(Graphics::Instance().GetDeviceContext());
        back_framebuffer_->Activate(Graphics::Instance().GetDeviceContext());
        RunPass(RenderPass::RenderPass_Background);
        back_framebuffer_->Deactivate(Graphics::Instance().GetDeviceContext());
    }
    
    //オブジェクト用フレームバッファ
    object_framebuffer_->Clear(Graphics::Instance().GetDeviceContext());
    object_framebuffer_->Activate(Graphics::Instance().GetDeviceContext());
    RunPass(RenderPass::RenderPass_Object);
    object_framebuffer_->Deactivate(Graphics::Instance().GetDeviceContext());


    //フルスクリーンクワッドで画面に転送
    ID3D11ShaderResourceView* srvs[] = {
        back_framebuffer_->GetShaderResourceView(0).Get(),
        object_framebuffer_->GetShaderResourceView(0).Get(),
    };
    Graphics::Instance().SetShaderResource(0, _countof(srvs), srvs);
    bit_block_transfer_->blit(
        Graphics::Instance().GetDeviceContext(),
        srvs,
        0,
        _countof(srvs) 
    );

    Graphics::Instance().ClearShaderResourceViews(0, 2);
}

//レンダーパスごとにシステムを回す
void RenderSystemManager::RunPass(RenderPass pass)
{
    for (auto& system : systems_)
    {
        if (system->GetPass() == pass)
        {
            system->Render();
        }
    }
        
}
