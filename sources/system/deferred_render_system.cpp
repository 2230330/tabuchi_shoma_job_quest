#include"../../headers/system/deferred_render_system.h"
#include"../../headers/constant_buffer_slot.h"
#include"../../headers/resource_manager.h"
#include"../../headers/graphics.h"

DeferredRenderSystem::DeferredRenderSystem(RenderPass render_pass)
    :IRenderSystem(render_pass)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
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

    //ŠÔÚŒõ
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
    //Ž©ŒÈ”­Œõ  
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