#include"../../headers/system/render_system_manager.h"

#include<dxgi1_6.h>

#include"../../headers/graphics.h"
#include"../../headers/misc.h"
#include"../../headers/render_state.h"
#include"../../headers/resource_manager.h"
#include"../../headers/light_manager.h"
#include"../../headers/fullscreen_quad.h"
#include"../../headers/framebuffer.h"
#include"../../headers/deferred_g_buffer.h"
#include"../../headers/system/render_gltf_system.h"
#include"../../headers/system/render_instancing_system.h"
#include"../../headers/system/render_sprite_system.h"
#include"../../headers/system/i_render_system.h"
#include"../../headers/system/render_sky_system.h"
#include"../../headers/system/render_cloud_system.h"
#include"../../headers/system/render_fog_system.h"
#include"../../headers/system/render_deferred_system.h"
#include"../../headers/system/render_screen_space_reflection_system.h"
#include"../../headers/system/render_golden_ratio_system.h"
#include"../../headers/system/ibl_manager.h"
#include"../../headers/system/camera_set_constants.h"
#include"../../headers/system/render_back_system.h"
#include"../../headers/system/render_sky_dx12_system.h"
#include"../../headers/system/render_cloud_dx12_system.h"
#include"../../headers/post_process/post_process_manager.h"
#include"../../headers/component/component_manager.h"
#include"../../headers/root_signature_manager.h"

//描画システムを管理するクラス
//描画システムは、背景、オブジェクト、UIなどの描画を担当する複数のシステムで構成されます。
//このクラスは、これらのシステムを管理し、必要に応じて更新や描画を行います。
//将来的には、描画システムの追加や削除、描画順序の管理などもこのクラスで行う予定です。
//現在は、背景用の描画システムとオブジェクト用の描画システムを管理しています。
//背景用の描画システムは、スカイレンダリングとクラウドレンダリングの2つで構成されており、
//オブジェクト用の描画システムは、デファードレンダリングの1つで構成されています。
RenderSystemManager::RenderSystemManager(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
    //AddSystem(std::make_unique<SkyRenderSystem>(comp_mng_,RenderPass_Background));
    //AddSystem(std::make_unique<CloudRenderSystem>(comp_mng_,RenderPass_Background));
    sky_render_system_ = std::make_unique<RenderSkySystem>(comp_mng_, RenderPass_Background);
    cloud_render_system_ = std::make_unique<RenderCloudSystem>(comp_mng_, RenderPass_Background);
    fog_render_system_ = std::make_unique<RenderFogSystem>(comp_mng_,RenderPass_Background);
    fog_render_system_->SetObjectResolution(Graphics::Instance().GetScreenWidth(), Graphics::Instance().GetScreenHeight());
    AddSystem(std::make_unique<GltfRenderSystem>(comp_mng_,RenderPass_Object));
    AddSystem(std::make_unique<InstancingRenderSystem>(comp_mng_,RenderPass_Object));
    AddSystem(std::make_unique<SpriteRenderSystem>(comp_mng_,RenderPass_Lighting));
    deferred_render_system_ = std::make_unique<RenderDeferredSystem>(comp_mng_, RenderPass_Lighting);
    ssr_render_system_ = std::make_unique<RenderScreenSpaceReflectionSystem>(comp_mng_, RenderPass_Lighting);
    golden_ratio_render_system_ = std::make_unique<RenderGoldenRatioSystem>(comp_mng_,RenderPass_UI);
	back_render_system_ = std::make_unique<RenderBackSystem>();
	sky_dx12_render_system_ = std::make_unique<RenderSkyDX12System>();
    cloud_render_dx12_system_ = std::make_unique<RenderCloudDX12System>();

    bit_block_transfer_ = std::make_unique<FullscreenQuad>(Graphics::Instance().GetDevice());
    sky_framebuffer_ = std::make_unique<FrameBuffer>(
        Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()/back_scale_),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight()/back_scale_)
    );
    cloud_framebuffer_ = std::make_unique<FrameBuffer>(
        Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()/back_scale_),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight()/back_scale_)
    );
    fog_framebuffer_ = std::make_unique<FrameBuffer>(
        Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()/2.f),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight()/2.f)
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

    //
    render_state_ = std::make_unique<RenderState>(Graphics::Instance().GetDevice());

    //IBLマネージャ
    ibl_manager_ = std::make_unique<IBLManager>();
    ibl_manager_->Initialize(Graphics::Instance().GetDevice());

    //カメラ情報の更新用クラス
    camera_set_constants_ = std::make_unique<CameraSetConstants>(comp_mng_);

    celestial_light_ps_ =
        ResourceManager::Instance().LoadPixelShader(Graphics::Instance().GetDevice(),
            L".\\resources\\shader\\celestial_light_ps.cso");
    light_shafts_ps_ =
        ResourceManager::Instance().LoadPixelShader(Graphics::Instance().GetDevice(),
            L".\\resources\\shader\\radial_god_lay_ps.cso");
    final_synthesis_ps_ =
        ResourceManager::Instance().LoadPixelShader(Graphics::Instance().GetDevice(),
            L".\\resources\\shader\\render_master_synthesis_ps.cso");
    
    post_process_manager_ = std::make_unique<PostProcessManager>(Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight()));

    RootSignatureManager::Instance();



	//DX12用の大気散乱描画用のテクスチャリソース
    ID3D12Device* device = Graphics::Instance().GetDX12Device();

	const UINT width = static_cast<UINT>(Graphics::Instance().GetScreenWidth() );
	const UINT height = static_cast<UINT>(Graphics::Instance().GetScreenHeight() );

    //画面サイズの2dテクスチャ
	D3D12_RESOURCE_DESC texture_desc{};
	texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.DepthOrArraySize = 1;
    texture_desc.MipLevels = 1;
	texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texture_desc.SampleDesc.Count = 1;
    texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    //GPU状に置くテクスチャリソース
    D3D12_HEAP_PROPERTIES herp_properties{};
    herp_properties.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = device->CreateCommittedResource(
        &herp_properties,
        D3D12_HEAP_FLAG_NONE,
        &texture_desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(dx12_sky_texture_.GetAddressOf())
    );
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    //子のテクスチャを指すRTVディスクリプタを作る
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.NumDescriptors = 1;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = device->CreateDescriptorHeap(
        &rtv_heap_desc,
        IID_PPV_ARGS(dx12_sky_rtv_heap_.GetAddressOf()));
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
        dx12_sky_rtv_heap_->GetCPUDescriptorHandleForHeapStart();

    device->CreateRenderTargetView(
        dx12_sky_texture_.Get(),
        nullptr,
        rtv_handle);

    //テクスチャを指すSRVとSRVディスクリプタを作成する
	D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc{};
    srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_heap_desc.NumDescriptors = 9;
    //GPU側から参照できるヒープとして
    srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    hr = device->CreateDescriptorHeap(
        &srv_heap_desc,
        IID_PPV_ARGS(dx12_sky_srv_heap_.GetAddressOf()));
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    const UINT descriptor_size =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D.MipLevels = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle =
        dx12_sky_srv_heap_->GetCPUDescriptorHandleForHeapStart();

    //まず、t0~t8を仮SRVで埋める
    for (int i = 0;i < 9;i++)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = cpu_handle;
        handle.ptr += static_cast<size_t>(i) * descriptor_size;

        //リソースがNULLのSRVを作成
        device->CreateShaderResourceView(nullptr, &srv_desc, handle);
    }

    //HLSLのregister(t4)にsky_textureを置く
    D3D12_CPU_DESCRIPTOR_HANDLE sky_handle = cpu_handle;
    sky_handle.ptr += static_cast<size_t>(4) * descriptor_size;

    device->CreateShaderResourceView(
        dx12_sky_texture_.Get(),
        &srv_desc,
        sky_handle
    );

 //   D3D12_CPU_DESCRIPTOR_HANDLE srv_handle =
 //       dx12_sky_srv_heap_->GetCPUDescriptorHandleForHeapStart();
	////このハンドルにsky_texture_を指すSRVを作成する
 //   D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
 //   srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
 //   srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
 //   //SRVを通してテクスチャの各色成分をどう読みだすかを指定する設定
 //   srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
 //   srv_desc.Texture2D.MipLevels = 1;
 //   device->CreateShaderResourceView(
 //       dx12_sky_texture_.Get(),
 //       &srv_desc,
 //       srv_handle);

}

//デストラクタ
RenderSystemManager::~RenderSystemManager() = default;


void RenderSystemManager::AddSystem(std::unique_ptr<IRenderSystem> system)
{
    systems_.emplace_back(std::move(system));
}


void RenderSystemManager::RenderAll()
{
    auto* ctx = Graphics::Instance().GetDeviceContext();

    //カメラ情報の更新
    camera_set_constants_->SetBuffer(ctx,light_manager_->GetDirectionLight().direction);

    //ライト情報の更新
    deferred_render_system_->SetLightManager(light_manager_);

    // === オブジェクト（毎フレーム） ===
    //オブジェクトの描画は毎フレーム行います。
    //背景より先に描画を行うのは、背景描画の際にオブジェクトの深度情報を利用する為です。
    deferred_framebuffer_->Clear(ctx);
    deferred_framebuffer_->Activate(ctx);
    RunPass(RenderPass::RenderPass_Object);
    deferred_framebuffer_->Deactivate(ctx);


    // === 背景（低頻度） ===
    bool sky_flag = sky_render_system_->GetSkyFlag();
    bool cloud_flag = cloud_render_system_->HasRenderableCloud();
    ID3D11ShaderResourceView* cloud_shadow_srv[] = { nullptr };

    if (back_sample_count_ < back_sample_limit_) {
        back_sample_count_++;
    }
    else {
        back_sample_count_ = 0;

        sky_framebuffer_->Clear(ctx);
        sky_framebuffer_->Activate(ctx);
        // 空、雲と別に描画し、作成した画像を雲に渡す
        sky_render_system_->Render();
        sky_framebuffer_->Deactivate(ctx);

        cloud_framebuffer_->Clear(ctx);
        cloud_framebuffer_->Activate(ctx);

        cloud_render_system_->SetSkyColorSRV(sky_framebuffer_->GetShaderResourceView(0).Get());
        //オブジェクトの深度情報を渡すことで、オブジェクトがある位置は雲が描画されないようにする(早期処理)
        cloud_render_system_->SetObjectDepthSRV(deferred_framebuffer_->GetSRV(Target::Depth));
        cloud_render_system_->SetObjectResolution(Graphics::Instance().GetScreenWidth()*obj_scale_, Graphics::Instance().GetScreenHeight()*obj_scale_);
        cloud_render_system_->Render();

        cloud_framebuffer_->Deactivate(ctx);
        
        back_framebuffer_->Clear(ctx);
        back_framebuffer_->Activate(ctx);
        //雲がなかった場合、大気をそのまま描画する
        if (!cloud_flag)
        {
            ID3D11ShaderResourceView* srv[] = { sky_framebuffer_->GetShaderResourceView(0).Get() };
            bit_block_transfer_->blit(ctx, srv, 0, 1);
        }

        // 天体光描画
        if (sky_flag)
        {
            ctx->OMSetBlendState(render_state_->GetBlendState(BlendState::additive), nullptr, 0xFFFFFFFF);

            if (cloud_flag)
            {
                cloud_shadow_srv[0] = {
                    cloud_framebuffer_->GetShaderResourceView(0).Get(),
                };
            }
            bit_block_transfer_->blit(ctx, cloud_shadow_srv, 0, 1, celestial_light_ps_.Get());
        }
        back_framebuffer_->Deactivate(ctx);

    }




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


    //オブジェクトのライティング
    for (int i = 0; i < Target::Count; i++)
    {
        ID3D11ShaderResourceView* srv = deferred_framebuffer_->GetSRV(i);
        deferred_render_system_->SetSRV(srv, i);
    }
    //object_framebuffer_->Clear(ctx);
    object_framebuffer_->Activate(ctx);
    //オブジェクトのライティング
    deferred_render_system_->Render();

    object_framebuffer_->Deactivate(ctx);

    //オブジェクトのライティング後にフォグの生成
    //影情報を用いて疑似的なライトシャフトを行うため
    fog_framebuffer_->Clear(ctx,FrameBuffer::usage::color,{0.f,1.0f,0.f,0.f});
    ctx->OMSetBlendState(render_state_->GetBlendState(BlendState::opaque), nullptr, 0xFFFFFFFF);
    fog_framebuffer_->Activate(ctx);
    fog_render_system_->Render();
    fog_render_system_->SetObjectDepthView(deferred_framebuffer_->GetSRV(Target::Depth));
    fog_framebuffer_->Deactivate(ctx);

    //SSRを行う
    ssr_render_system_->SetNormalSRV(deferred_framebuffer_->GetSRV(Target::Normal));
    ssr_render_system_->SetDepthSRV(deferred_framebuffer_->GetSRV(Target::Depth));
    ssr_render_system_->SetColorSRV(object_framebuffer_->GetShaderResourceView(0).Get());
    ssr_render_system_->SetDepthTex(deferred_framebuffer_->GetDepthTex());
    ssr_render_system_->Render();

    // === 合成
    final_framebuffer_->Clear(ctx);
    final_framebuffer_->Activate(ctx);


    ID3D11ShaderResourceView* srvs[] = {
        back_framebuffer_->GetShaderResourceView(0).Get(),
        ssr_render_system_->GetSSRTexture(),
        deferred_framebuffer_->GetSRV(Target::Depth),
        fog_framebuffer_->GetShaderResourceView(0).Get(),
    };
    Graphics::Instance().SetShaderResource(0, _countof(srvs), srvs);
    bit_block_transfer_->blit(ctx, srvs, 0, _countof(srvs),final_synthesis_ps_.Get());



    final_framebuffer_->Deactivate(ctx);
    Graphics::Instance().ClearShaderResourceViews(0, _countof(srvs));

    // === ポストプロセス ===
    post_process_manager_->SetEmissiveMap(deferred_framebuffer_->GetSRV(Target::Emissive));
    post_process_manager_->PostProcess(ctx, final_framebuffer_->GetShaderResourceView(0).Get());

    ////最終結果を描画
    Graphics::Instance().ViewClear(0, 0, 0, 0);
    Graphics::Instance().SetRenderTargets(); //FXAAでフルスクリーン描画するため、コンテキストをリセット
    ctx->OMSetBlendState(render_state_->GetBlendState(BlendState::transparency),nullptr,0xFFFFFFFF);
    bit_block_transfer_->blit(ctx, post_process_manager_->GetResultShaderResourceView().GetAddressOf(), 0, 1);

    //黄金分割構図を描画
    ctx->OMSetBlendState(render_state_->GetBlendState(BlendState::additive), nullptr, 0xFFFFFFFF);
    golden_ratio_render_system_->Render();
}

//DX12用の描画関数
void RenderSystemManager::RenderAllDX12()
{
    //コマンドリストの取得
	ID3D12GraphicsCommandList* cmd_list = Graphics::Instance().GetCommandList();

    //ルートシグネチャのセット
    cmd_list->SetGraphicsRootSignature(RootSignatureManager::Instance().GetRootSignature3D().Get());

	//ライト情報の更新
	deferred_render_system_->SetLightManager(light_manager_);

	//カメラ情報の更新
	camera_set_constants_->SetBufferDX12(cmd_list, light_manager_->GetDirectionLight().direction);

    //テクスチャリソースをsky_texture_にする
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dx12_sky_texture_.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	cmd_list->ResourceBarrier(1, &barrier);

	//これからのDrawの書き込み先をsky_texture_にする
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = dx12_sky_rtv_heap_->GetCPUDescriptorHandleForHeapStart();
	cmd_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

    sky_dx12_render_system_->RenderDX12(cmd_list);

	//sky_texture_を元の状態に戻す
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dx12_sky_texture_.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    cmd_list->ResourceBarrier(1, &barrier);

    Graphics::Instance().SetBackBufferRenderTargetDX12();

    //空の画像情報を雲シェーダーで読み込むため、
    //雲の描画前にテーブルを設定する
    ID3D12DescriptorHeap* heaps[] = {
        dx12_sky_srv_heap_.Get()
    };
    cmd_list->SetDescriptorHeaps(_countof(heaps), heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE srv_table =
        dx12_sky_srv_heap_->GetGPUDescriptorHandleForHeapStart();
    cmd_list->SetGraphicsRootDescriptorTable(
        RootSignatureSlot::kDx12TextureTable,
        srv_table
    );


    //雲描画システムに空のSRVを登録
    cloud_render_dx12_system_->GetSkyTexture(dx12_sky_texture_.Get());

    cloud_render_dx12_system_->RenderDX12(cmd_list);
    

}

void RenderSystemManager::SetLightManager(LightManager* light_manager)
{
    this->light_manager_ = light_manager;
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
