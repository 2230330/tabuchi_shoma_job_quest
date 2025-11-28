#include"../../headers/system/render_system_manager.h"

#include"../../headers/graphics.h"
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
    AddSystem(std::make_unique<RenderSystem>(comp_mng_,RenderPass_Object));
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
    back_framebuffer_->Clear(Graphics::Instance().GetDeviceContext());
    back_framebuffer_->Activate(Graphics::Instance().GetDeviceContext());
    RunPass(RenderPass::RenderPass_Background);
    back_framebuffer_->Deactivate(Graphics::Instance().GetDeviceContext());

    //オブジェクト用フレームバッファ
    object_framebuffer_->Clear(Graphics::Instance().GetDeviceContext());
    object_framebuffer_->Activate(Graphics::Instance().GetDeviceContext());
    bit_block_transfer_->blit(
        Graphics::Instance().GetDeviceContext(),
        back_framebuffer_->GetShaderResourceView(0).GetAddressOf(),
        0,
        1
    );
    RunPass(RenderPass::RenderPass_Object);
    object_framebuffer_->Deactivate(Graphics::Instance().GetDeviceContext());

    bit_block_transfer_->blit(
        Graphics::Instance().GetDeviceContext(),
        object_framebuffer_->GetShaderResourceView(0).GetAddressOf(),
        0,
        1
    );
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
