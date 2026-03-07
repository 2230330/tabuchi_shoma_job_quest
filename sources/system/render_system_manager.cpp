#include"../../headers/system/render_system_manager.h"

#include"../../headers/graphics.h"
#include"../../headers/render_state.h"
#include"../../headers/system/render_gltf_system.h"
#include"../../headers/system/instancing_render_system.h"
#include"../../headers/system/sprite_render_system.h"
#include"../../headers/resource_manager.h"



RenderSystemManager::RenderSystemManager(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
    //AddSystem(std::make_unique<SkyRenderSystem>(comp_mng_,RenderPass_Background));
    //AddSystem(std::make_unique<CloudRenderSystem>(comp_mng_,RenderPass_Background));
    sky_render_system_ = std::make_unique<SkyRenderSystem>(comp_mng_, RenderPass_Background);
    cloud_render_system_ = std::make_unique<CloudRenderSystem>(comp_mng_, RenderPass_Background);
    AddSystem(std::make_unique<GltfRenderSystem>(comp_mng_,RenderPass_Object));
    AddSystem(std::make_unique<InstancingRenderSystem>(comp_mng_,RenderPass_Object));
    AddSystem(std::make_unique<SpriteRenderSystem>(comp_mng_,RenderPass_Lighting));
    deferred_render_system_ = std::make_unique<DeferredRenderSystem>( RenderPass_Lighting);


    bit_block_transfer_ = std::make_unique<FullscreenQuad>(Graphics::Instance().GetDevice());
    sky_framebuffer_ = std::make_unique<FrameBuffer>(
        Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()/back_scale_),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight()/back_scale_)
    );
    back_framebuffer_ = std::make_unique<FrameBuffer>(
        Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()/back_scale_),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight()/back_scale_)//背景は小さく描画、アップサンプリングで合成
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
    deferred_framebuffer_ = std::make_unique<DeferredGBuffer>(
        Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight())
    );

    //IBLマネージャ
    ibl_manager_ = std::make_unique<IBLManager>();
    ibl_manager_->Initialize(Graphics::Instance().GetDevice());

    celestial_light_ps_ =
        ResourceManager::Instance().LoadPixelShader(Graphics::Instance().GetDevice(),
            L".\\resources\\shader\\celestial_light_ps.cso");
    light_shafts_ps_ =
        ResourceManager::Instance().LoadPixelShader(Graphics::Instance().GetDevice(),
            L".\\resources\\shader\\radial_god_lay_ps.cso");
    fxaa_ps_ =
        ResourceManager::Instance().LoadPixelShader(Graphics::Instance().GetDevice(),
            L".\\resources\\shader\\fast_approximate_anti_aliasing_ps.cso");

}

void RenderSystemManager::AddSystem(std::unique_ptr<IRenderSystem> system)
{
    systems_.emplace_back(std::move(system));
}


void RenderSystemManager::RenderAll()
{
    auto* ctx = Graphics::Instance().GetDeviceContext();
    deferred_render_system_->SetLightManager(light_manager_);

    // === オブジェクト（毎フレーム） ===
    deferred_framebuffer_->Clear(ctx);
    deferred_framebuffer_->Activate(ctx);

    RunPass(RenderPass::RenderPass_Object);
    deferred_framebuffer_->Deactivate(ctx);


    // === 背景（低頻度） ===
    bool sky_flag = sky_render_system_->GetSkyFlag();
    bool cloud_flag = cloud_render_system_->HasRenderableCloud();

    sky_framebuffer_->Clear(ctx);
    sky_framebuffer_->Activate(ctx);
    // 空、雲と別に描画し、作成した画像を雲に渡す
    sky_render_system_->Render();

    if (back_sample_count_ < back_sample_rimit_) {
        back_sample_count_++;
    }
    else {
        back_sample_count_ = 0;

        sky_framebuffer_->Deactivate(ctx);

        back_framebuffer_->Clear(ctx);
        back_framebuffer_->Activate(ctx);

        cloud_render_system_->SetSkyColorSRV(sky_framebuffer_->GetShaderResourceView(0).Get());
        cloud_render_system_->Render();

        
        //雲がなかった場合、大気をそのまま描画する
        if (!cloud_flag)
        {
            ID3D11ShaderResourceView* srv[] = { sky_framebuffer_->GetShaderResourceView(0).Get() };
            bit_block_transfer_->blit(ctx, srv, 0, 1);
        }
        
        //雲オンリーのゴッドレイ(未完成)
        //if (cloud_flag)
        //{
        //    srvs[0] = {
        //        cloud_render_system_->GetCloudShadowSRV(),
        //    };
        //}
        //bit_block_transfer_->blit(ctx, srvs, 0, 1, light_shafts_ps_.Get());
    }
        // 天体光描画
    if (sky_flag)
    {
        ID3D11ShaderResourceView* cloud_shadow_srv[] = { nullptr };
        if(cloud_flag)
        {
            cloud_shadow_srv[0] = {
                cloud_render_system_->GetCloudShadowSRV(),
            };
        }
        bit_block_transfer_->blit(ctx, cloud_shadow_srv, 0, 1, celestial_light_ps_.Get());
    }

    back_framebuffer_->Deactivate(ctx);

    //スカイキューブ作成
            // IBL 入力更新（背景SRV→SkyCube化を内包）
    if (sky_flag || cloud_flag)
    {
        ibl_manager_->SetSkyFlag(sky_flag);
        ibl_manager_->SetCloudFlag(cloud_flag);
        ibl_manager_->BuildSkyCubeFromEnvSource();

        if (ibl_manager_->IsDirty()) 
        {
            // Specularの分割更新（負荷に応じて複数ステップ回すと収束が早い）
            //for (int s = 0; s < ibl_steps_per_frame_; ++s) 
            {
                ibl_manager_->UpdateSpecularPrefilter();
            }

            // Diffuse SH
            ibl_manager_->UpdateDiffuseSH();
            Graphics::Instance().SetRenderTargets(); //IBL更新でコンテキストが汚れるのでリセット
        }

        // IBL を PS にバインド（t1: PrefEnv, t2: BRDF LUT, b2: SH）
        ibl_manager_->BindForObjectPass(ctx);
    }


    for (int i = 0; i < DeferredGBuffer::Target::Count; i++)
    {
        deferred_render_system_->SetSRV(deferred_framebuffer_->GetSRV(i), i);
    }

    object_framebuffer_->Clear(ctx);
    object_framebuffer_->Activate(ctx);
    deferred_render_system_->Render();

    object_framebuffer_->Deactivate(ctx);

    // === 合成
    
    final_framebuffer_->Clear(ctx);
    final_framebuffer_->Activate(ctx);

    ID3D11ShaderResourceView* srvs[] = {
        back_framebuffer_->GetShaderResourceView(0).Get(),
        object_framebuffer_->GetShaderResourceView(0).Get(),
    };
    Graphics::Instance().SetShaderResource(0, _countof(srvs), srvs);
    bit_block_transfer_->blit(ctx, srvs, 0, _countof(srvs));
    final_framebuffer_->Deactivate(ctx);
    Graphics::Instance().ClearShaderResourceViews(0, _countof(srvs));

    ////FXAAによるアンチエイジング
    Graphics::Instance().ViewClear(0, 0, 0, 0);
    Graphics::Instance().SetRenderTargets(); //FXAAでフルスクリーン描画するため、コンテキストをリセット
    RenderState render_state(Graphics::Instance().GetDevice());
    ctx->OMSetBlendState(render_state.GetBlendState(BlendState::transparency),nullptr,0xFFFFFFFF);

    ID3D11ShaderResourceView* fxaa_srvs[]
    {
        final_framebuffer_->GetShaderResourceView(0).Get(),
    };
    Graphics::Instance().SetShaderResource(0, 1, fxaa_srvs);
    bit_block_transfer_->
        blit(ctx, fxaa_srvs, 0, 1, fxaa_ps_.Get());

    history_color_srv_ = final_framebuffer_->GetShaderResourceView(0).Get();
    Graphics::Instance().ClearShaderResourceViews(0,1);

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
