#include"../../headers/system/deferred_render_system.h"
#include"../../headers/constant_buffer_slot.h"
#include"../../headers/resource_manager.h"
#include"../../headers/graphics.h"
#include"../../headers/scene/scene.h"
#include"../../headers/misc.h"


DeferredRenderSystem::DeferredRenderSystem(ComponentManager&comp_mng,RenderPass render_pass)
    :comp_mng_(comp_mng)
    ,IRenderSystem(render_pass)
{
    HRESULT hr{ S_OK };
    ID3D11Device* device = Graphics::Instance().GetDevice();
    //	シャドウマップ準備
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_buffer{};
        D3D11_TEXTURE2D_DESC texture2d_desc{};
        texture2d_desc.Width = shadowmap_width_;
        texture2d_desc.Height = shadowmap_height_;
        texture2d_desc.MipLevels = 1;
        texture2d_desc.ArraySize = 1;
        texture2d_desc.Format = DXGI_FORMAT_R32_TYPELESS;
        texture2d_desc.SampleDesc.Count = 1;
        texture2d_desc.SampleDesc.Quality = 0;
        texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
        texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        texture2d_desc.CPUAccessFlags = 0;
        texture2d_desc.MiscFlags = 0;
        hr = device->CreateTexture2D(&texture2d_desc, NULL, depth_buffer.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        //	深度ステンシルビュー生成
        D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
        depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT;
        depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        depth_stencil_view_desc.Texture2D.MipSlice = 0;
        hr = device->CreateDepthStencilView(depth_buffer.Get(),
            &depth_stencil_view_desc,
            shadowmap_depth_stencil_view_.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        //	シェーダーリソースビュー生成
        D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
        shader_resource_view_desc.Format = DXGI_FORMAT_R32_FLOAT;
        shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
        shader_resource_view_desc.Texture2D.MipLevels = 1;
        hr = device->CreateShaderResourceView(depth_buffer.Get(),
            &shader_resource_view_desc,
            shadowmap_shader_resource_view_.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    //定数バッファの生成
    {
        D3D11_BUFFER_DESC buffer_desc{};
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = 0;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;
        {
            buffer_desc.ByteWidth = (sizeof(scene_constants) + 15) / 16 * 16;
            hr = Graphics::Instance().GetDevice()->CreateBuffer(&buffer_desc, nullptr, shadow_scene_constant_buffer_.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }

    }
    //フレームバッファの生成
    {

        shadowmap_framebuffer_ = std::make_unique<FrameBuffer>(
            device,shadowmap_width_,shadowmap_height_,FrameBuffer::usage::depth,true);
    }

    deferred_rendering_directional_ps_ =
        ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\deferred_rendering_lighting_ps.cso");
    deferred_rendering_indirect_ps_ =
        ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\deferred_rendering_indirect_ps.cso");
    deferred_rendering_emissive_ps_ =
        ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\deferred_rendering_emissive_ps.cso");

    fullscreen_quad_ = std::make_unique<FullscreenQuad>(device);
    render_state_ = std::make_unique<RenderState>(device);
}

void DeferredRenderSystem::Render()
{
    ID3D11DeviceContext* ctx = Graphics::Instance().GetDeviceContext();

    //間接光
    {
        ctx->OMSetBlendState(render_state_->GetBlendState(BlendState::opaque), nullptr, 0xFFFFFFFF);


        ID3D11ShaderResourceView* srvs[] =
        {
            srvs_[DeferredGBuffer::Target::BaseColor].Get(),
            srvs_[DeferredGBuffer::Target::Emissive].Get(),
            srvs_[DeferredGBuffer::Target::Normal].Get(),
            srvs_[DeferredGBuffer::Target::Parameter].Get(),
            srvs_[DeferredGBuffer::Target::Depth].Get(),
            srvs_[DeferredGBuffer::Target::Velocity].Get(),
        };
        fullscreen_quad_->blit(ctx, srvs, 0, _countof(srvs), deferred_rendering_indirect_ps_.Get());
    }
    //自己発光  
    {
        ctx->OMSetBlendState(render_state_->GetBlendState(BlendState::additive), nullptr, 0xFFFFFFFF);


        ID3D11ShaderResourceView* srvs[] =
        {
            srvs_[DeferredGBuffer::Target::BaseColor].Get(),
            srvs_[DeferredGBuffer::Target::Emissive].Get(),
            srvs_[DeferredGBuffer::Target::Normal].Get(),
            srvs_[DeferredGBuffer::Target::Parameter].Get(),
            srvs_[DeferredGBuffer::Target::Depth].Get(),
            srvs_[DeferredGBuffer::Target::Velocity].Get(),
        };
        fullscreen_quad_->blit(ctx, srvs, 0, _countof(srvs), deferred_rendering_emissive_ps_.Get());
    }

    directional_shadow_rendering();

    const auto size = light_manager_->GetDeferredLightsSize();
    for (int i = 0; i < size; i++)
    {
        ctx->OMSetBlendState(render_state_->GetBlendState(BlendState::additive), nullptr, 0xFFFFFFFF);

        ID3D11ShaderResourceView* srvs[] =
        {
            srvs_[DeferredGBuffer::Target::BaseColor].Get(),
            srvs_[DeferredGBuffer::Target::Emissive].Get(),
            srvs_[DeferredGBuffer::Target::Normal].Get(),
            srvs_[DeferredGBuffer::Target::Parameter].Get(),
            srvs_[DeferredGBuffer::Target::Depth].Get(),
            srvs_[DeferredGBuffer::Target::Velocity].Get(),
        };

        light_manager_->BindDeferredLightConstant(ConstantBufferSlot::kDeferredLight, i);
        //影情報の送信
        ID3D11ShaderResourceView*shadowmap= shadowmap_framebuffer_->GetShaderResourceView(1).Get();
        ctx->PSSetShaderResources(10, 1, &shadowmap);

        fullscreen_quad_->blit(ctx, srvs, 0,_countof(srvs), deferred_rendering_directional_ps_.Get());
    }
}

void DeferredRenderSystem::directional_shadow_rendering()
{
    //定数バッファの更新
    shadowmap_framebuffer_->Clear(Graphics::Instance().GetDeviceContext(), FrameBuffer::usage::depth_stencil);
    shadowmap_framebuffer_->Activate(Graphics::Instance().GetDeviceContext(), FrameBuffer::usage::depth_stencil);
    {

    //ライト方向から見た視線行列を生成

    DirectX::XMVECTOR light_dir =
        DirectX::XMVector3Normalize(DirectX::XMLoadFloat4(&light_manager_->GetDirectionLight().direction));
    
    // 注視点は中心
    DirectX::XMVECTOR target;

    //カメラ位置を注視点にすることで、カメラ周辺にシャドウマップの中心が来るようにする
    target = DirectX::XMLoadFloat4(&camera_position_);

    //// 影中心からライト方向に引いた位置にライトを置く
    DirectX::XMVECTOR light_pos =
        DirectX::XMVectorSubtract(
            target,
            DirectX::XMVectorScale(light_dir, shadow_distance_) // target - light_dir * dist
        );


    // up が light_dir と平行に近い場合の対策
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f);
    if (fabs(DirectX::XMVectorGetX(DirectX::XMVector3Dot(light_dir, up))) > 0.95f)
        up = DirectX::XMVectorSet(0.f, 0.f, 1.f, 0.f);

    DirectX::XMMATRIX V0 = DirectX::XMMatrixLookAtLH(light_pos, target, up);
    DirectX::XMStoreFloat4x4(&light_view_, V0);


    // === 直交投影（w,h は“ワールド距離”）===
    float w = shadow_coverage_;
    float h = shadow_coverage_;

    //テクセルスナップで泳ぎ防止
    DirectX::XMMATRIX V;
    {
        // ライトビュー空間の 1 テクセルサイズ
        float texel_size_x = w / static_cast<float>(shadowmap_width_);
        float texel_size_y = h / static_cast<float>(shadowmap_height_);

        // target をライトビューに変換→XY をグリッドへ丸め→ライト位置を補正
        DirectX::XMVECTOR targetL = DirectX::XMVector3TransformCoord(target, V0);
        float cx = DirectX::XMVectorGetX(targetL);
        float cy = DirectX::XMVectorGetY(targetL);
        cx = std::floor(cx / texel_size_x) * texel_size_x;
        cy = std::floor(cy / texel_size_y) * texel_size_y;

        // 丸めた中心がライトビューで (cx,cy) になるように、ビュー行列の原点を平行移動
        //DirectX::XMMATRIX off =
        //    DirectX::XMMatrixTranslation(-(cx - DirectX::XMVectorGetX(targetL)),
        //        -(cy - DirectX::XMVectorGetY(targetL)), 0.0f);


        //DirectX::XMMATRIX off = DirectX::XMMatrixTranslation(
        //    cx - DirectX::XMVectorGetX(targetL),
        //    cy - DirectX::XMVectorGetY(targetL),
        //    0.0f
        //);

        //V0 = off*V0; // 中心をグリッドに合わせる

        //スナップ差分をライト位置に反映
        DirectX::XMVECTOR delta = DirectX::XMVectorSet(
            DirectX::XMVectorGetX(targetL) - cx,
            DirectX::XMVectorGetY(targetL) - cy,
            0.0f, 0.0f);

        // ライト位置をスナップ差分だけ移動
        delta = DirectX::XMVector3TransformNormal(delta, DirectX::XMMatrixInverse(nullptr, V0));
        //ライト位置を補正
        light_pos = DirectX::XMVectorSubtract(light_pos, delta);

        // ビュー行列を再計算
        V = DirectX::XMMatrixLookAtLH(light_pos, target, up);
    }

    DirectX::XMMATRIX P = DirectX::XMMatrixOrthographicLH(
        w, h,
        shadow_near_clip_plane_, shadow_far_clip_plane_
    );

    DirectX::XMStoreFloat4x4(&light_projection_, P);

    DirectX::XMMATRIX VP = V * P;
    DirectX::XMStoreFloat4x4(&light_view_projection_, VP);
    DirectX::XMStoreFloat4x4(&inverse_light_view_projection_, DirectX::XMMatrixInverse(nullptr, VP));


    //ここでシーンの定数バッファを更新する必要がある
    ShadowSceneConstants constants{};
    constants.light_view_projection = light_view_projection_;
    constants.inverse_light_view_projection = inverse_light_view_projection_;
    Graphics::Instance().GetDeviceContext()->UpdateSubresource(
        shadow_scene_constant_buffer_.Get(), 0, nullptr, &constants, 0, 0);
    Graphics::Instance().SetConstantBuffer(
        ConstantBufferSlot::kShadowMap, 1, shadow_scene_constant_buffer_.GetAddressOf());

    //通常GLTFモデルのシャドウマップレンダリング
    comp_mng_.ForEach<ComponentGltf>([this](uint32_t entity_id, ComponentGltf& gltf)
        {
            if (!comp_mng_.Has<ComponentSkyAtmosphere>(entity_id) &&
                !comp_mng_.Has<ComponentVolumetricCloud>(entity_id))
            {
                auto* l2w = comp_mng_.TryGetByEntity < ComponentLocalToWorld>(entity_id);
                auto* ins = comp_mng_.TryGetByEntity<ComponentInstanced>(entity_id);

                //要求したものがあったら
                if (l2w && !ins)
                {
                    auto* adjast = comp_mng_.TryGetByEntity<ComponentAdjastPbrParamter>(entity_id);
                    if (adjast)
                    {
                        gltf.model->SetAdjastParam(
                            adjast->adjust_metalness,
                            adjast->adjust_roughness);
                    }

                    gltf.model->Render(Graphics::Instance().GetDeviceContext(), l2w->value,true/*shadow_render_flag*/);
                }


            }

        });
    // モデルごとにインスタンスをグループ化
    {
        std::unordered_map<GltfModel*, std::vector<DirectX::XMFLOAT4X4>> model_to_worlds;

        comp_mng_.ForEach<ComponentGltf>([&](uint32_t entity_id, ComponentGltf& gltf)
            {
                if (!comp_mng_.Has<ComponentSkyAtmosphere>(entity_id) &&
                    !comp_mng_.Has<ComponentVolumetricCloud>(entity_id))
                {

                    auto* l2w = comp_mng_.TryGetByEntity<ComponentLocalToWorld>(entity_id);
                    auto* instanced = comp_mng_.TryGetByEntity<ComponentInstanced>(entity_id);

                    // インスタンシング対象のみ抽出
                    if (l2w && instanced)
                    {
                        model_to_worlds[gltf.model.get()].push_back(l2w->value);
                    }
                }
            });

        ID3D11Device* device = Graphics::Instance().GetDevice();
        ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();
        HRESULT hr{ S_OK };

        for (auto& [model, world_matrices] : model_to_worlds)
        {
            if (world_matrices.empty()) continue;


            InstanceBufferInfo& buf_info = instance_buffer_pool_[model];

            const UINT required_size = sizeof(DirectX::XMFLOAT4X4) * static_cast<UINT>(world_matrices.size());

            // 必要に応じて再生成（足りないときだけ）
            if (!buf_info.buffer || buf_info.cepasity < world_matrices.size())
            {
                D3D11_BUFFER_DESC desc{};
                desc.Usage = D3D11_USAGE_DYNAMIC;
                desc.ByteWidth = std::max(required_size, 16u);
                desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

                hr = device->CreateBuffer(&desc, nullptr, buf_info.buffer.ReleaseAndGetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), L"インスタンスバッファの作成に失敗しました");
                buf_info.cepasity = world_matrices.size();
            }

            // Map でデータ更新
            D3D11_MAPPED_SUBRESOURCE mapped{};
            hr = context->Map(buf_info.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
            if (SUCCEEDED(hr))
            {
                memcpy(mapped.pData, world_matrices.data(), required_size);
                context->Unmap(buf_info.buffer.Get(), 0);
            }

            // インスタンシング描画呼び出し
            model->InstancingRender(context,
                static_cast<UINT>(world_matrices.size()),
                buf_info.buffer.Get(),
                0, true/*shadow_render_flag*/);
        }
    }




    }
    shadowmap_framebuffer_->Deactivate(Graphics::Instance().GetDeviceContext());


}
