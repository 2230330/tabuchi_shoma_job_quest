#include"../../headers/system/render_system_manager.h"

#include"../../headers/graphics.h"
#include"../../headers/render_state.h"
#include"../../headers/system/render_system.h"
#include"../../headers/system/instancing_render_system.h"
#include"../../headers/system/sprite_render_system.h"



RenderSystemManager::RenderSystemManager(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
    //AddSystem(std::make_unique<SkyRenderSystem>(comp_mng_,RenderPass_Background));
    //AddSystem(std::make_unique<CloudRenderSystem>(comp_mng_,RenderPass_Background));
    sky_render_system_ = std::make_unique<SkyRenderSystem>(comp_mng_, RenderPass_Background);
    cloud_render_system_ = std::make_unique<CloudRenderSystem>(comp_mng_, RenderPass_Background);
    AddSystem(std::make_unique<GltfRenderSystem>(comp_mng_,RenderPass_Object));
    AddSystem(std::make_unique<InstancingRenderSystem>(comp_mng_,RenderPass_Object));
    AddSystem(std::make_unique<SpriteRenderSystem>(comp_mng_,RenderPass_Object));

    bit_block_transfer_ = std::make_unique<FullscreenQuad>(Graphics::Instance().GetDevice());
    sky_framebuffer_ = std::make_unique<FrameBuffer>(
        Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight())
    );
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

        // 空、雲と別に描画し、作成した画像を雲に渡す
        sky_framebuffer_->Clear(ctx);
        sky_framebuffer_->Activate(ctx);
        sky_render_system_->Render();
        sky_framebuffer_->Deactivate(ctx);

        back_framebuffer_->Clear(ctx);
        back_framebuffer_->Activate(ctx);

        cloud_render_system_->SetSkyColorSRV(sky_framebuffer_->GetShaderResourceView(0).Get());
        cloud_render_system_->Render();

        //雲がなかった場合、大気をそのまま描画する
        if (!cloud_render_system_->HasRenderableCloud())
        {
            ID3D11ShaderResourceView* srv[] = { sky_framebuffer_->GetShaderResourceView(0).Get() };
            bit_block_transfer_->blit(ctx, srv, 0, 1);
        }

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
    Graphics::Instance().ClearShaderResourceViews(0, _countof(srvs));

    //final_framebuffer_->Deactivate(ctx);

    //// 最終出力へ
    //srvs[0] = final_framebuffer_->GetShaderResourceView(0).Get();
    //Graphics::Instance().SetShaderResource(0, 1, srvs);
    //bit_block_transfer_->blit(ctx, srvs, 0, 1);
    //Graphics::Instance().ClearShaderResourceViews(0, 1);
}

//レンダーパスごとにシステムを回す
//背景は工程が多く複雑なので、こちらではオブジェクトパスのみを実装
void RenderSystemManager::RunPass(RenderPass pass)
{
    if (pass == RenderPass_Object)
    {
        for (auto& system : systems_)
        {
            if (system->GetPass() == pass)
            {
                system->Render();
            }
        }
    }

}
