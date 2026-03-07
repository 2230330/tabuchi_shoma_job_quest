#include"../../headers/system/deferred_render_system.h"
#include"../../headers/constant_buffer_slot.h"
#include"../../headers/resource_manager.h"
#include"../../headers/graphics.h"
#include"../../headers/scene/scene.h"
#include"../../headers/misc.h"


DeferredRenderSystem::DeferredRenderSystem(RenderPass render_pass)
    :IRenderSystem(render_pass)
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

        shadowmap_framebuffer_ = std::make_unique<FrameBuffer>(
            device,shadowmap_width_,shadowmap_height_,FrameBuffer::usage::depth,true);
    }

    deferred_rendering_directional_ps_ =
        ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\deferred_rendering_directional_ps.cso");
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

        fullscreen_quad_->blit(ctx, srvs, 0,_countof(srvs), deferred_rendering_directional_ps_.Get());
    }
}

void DeferredRenderSystem::directional_shadow_rendering()
{
    //定数バッファの更新
    shadowmap_framebuffer_->Clear(Graphics::Instance().GetDeviceContext(),FrameBuffer::usage::depth);
    shadowmap_framebuffer_->Activate(Graphics::Instance().GetDeviceContext(), FrameBuffer::usage::depth);

    //ここでシーンの定数バッファを更新する必要がある

    shadowmap_framebuffer_->Deactivate(Graphics::Instance().GetDeviceContext());
}
