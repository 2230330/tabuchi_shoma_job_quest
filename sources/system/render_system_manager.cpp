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
    final_framebuffer_ = std::make_unique<FrameBuffer>(
        Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight())
    );

    //IBLマネージャ
    ibl_manager_ = std::make_unique<IBLManager>();
    ibl_manager_->Initialize(Graphics::Instance().GetDevice());
}

void RenderSystemManager::AddSystem(std::unique_ptr<IRenderSystem> system)
{
    systems_.emplace_back(std::move(system));
}


void RenderSystemManager::RenderAll()
{
    auto* ctx = Graphics::Instance().GetDeviceContext();

    // === 背景（低頻度） ===
    if (back_sample_count_ < back_sample_rimit_) {
        back_sample_count_++;
    }
    else {
        back_sample_count_ = 0;

        //  背景パスに入る前に SkyRenderSystem に sky cube を渡す
        //    systems_ を走査して SkyRenderSystem を見つける（1回見つけたらキャッシュでもOK）
        for (auto& sys : systems_) {
            if (sys->GetPass() == RenderPass::RenderPass_Background) {
                if (auto sky = dynamic_cast<SkyRenderSystem*>(sys.get())) {
                    sky->SetSkyCubeSRV(ibl_manager_->GetSkyCubeSRV()); //  IBL の sky cube
                }
            }
        }

        back_framebuffer_->Clear(ctx);
        back_framebuffer_->Activate(ctx);
        RunPass(RenderPass::RenderPass_Background); // Sky は今渡した SRV を使って描画
        back_framebuffer_->Deactivate(ctx);

        // IBL 入力更新（背景SRV→SkyCube化を内包）
        ibl_manager_->UpdateEnvironmentCapture(*back_framebuffer_);
        ibl_manager_->BuildSkyCubeFromEnvSource();

        if (ibl_manager_->IsDirty()) {
            // Diffuse SH（軽い）
            ibl_manager_->UpdateDiffuseSH();

            // Specularの分割更新（負荷に応じて複数ステップ回すと収束が早い）
            for (int s = 0; s < ibl_steps_per_frame_; ++s) {
                ibl_manager_->UpdateSpecularPrefilter();
            }

            Graphics::Instance().SetRenderTargets(); //IBL更新でコンテキストが汚れるのでリセット
        }
    }

    // === オブジェクト（毎フレーム） ===
    object_framebuffer_->Clear(ctx);
    object_framebuffer_->Activate(ctx);

    // IBL を PS にバインド（t1: PrefEnv, t2: BRDF LUT, b2: SH）
    ibl_manager_->BindForObjectPass(ctx);

    RunPass(RenderPass::RenderPass_Object);
    object_framebuffer_->Deactivate(ctx);

    // === 合成
    //final_framebuffer_->Clear(ctx);
    //final_framebuffer_->Activate(ctx);

    ID3D11ShaderResourceView* srvs[] = {
        back_framebuffer_->GetShaderResourceView(0).Get(),
        object_framebuffer_->GetShaderResourceView(0).Get(),
    };
    Graphics::Instance().SetShaderResource(0, _countof(srvs), srvs);
    bit_block_transfer_->blit(ctx, srvs, 0, _countof(srvs));
    Graphics::Instance().ClearShaderResourceViews(0, 2);

    //final_framebuffer_->Deactivate(ctx);

    //// 最終出力へ
    //srvs[0] = final_framebuffer_->GetShaderResourceView(0).Get();
    //Graphics::Instance().SetShaderResource(0, 1, srvs);
    //bit_block_transfer_->blit(ctx, srvs, 0, 1);
    //Graphics::Instance().ClearShaderResourceViews(0, 1);
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
