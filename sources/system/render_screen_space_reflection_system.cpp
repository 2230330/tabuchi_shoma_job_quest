#include"../../headers/system/render_screen_space_reflection_system.h"

#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
#include"../../headers/constant_buffer_slot.h"
#include"../../headers/misc.h"
ScreenSpaceReflectionRenderSystem::ScreenSpaceReflectionRenderSystem(ComponentManager& comp_mng, RenderPass render_pass)
    :comp_mng_(comp_mng)
    ,IRenderSystem(render_pass)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
    HRESULT hr{ S_OK };
    //スクリーンスペースリフレクションの定数バッファの作成
    {
        D3D11_BUFFER_DESC buffer_desc{};
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = 0;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;
        {
            buffer_desc.ByteWidth = (sizeof(SsrConstants) + 15) / 16 * 16;
            hr = Graphics::Instance().GetDevice()->CreateBuffer(&buffer_desc, nullptr, ssr_constant_buffer_.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }
    }

    ssr_ps_ = 
        ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\screen_space_reflection_ps.cso");

    ssr_framebuffer_ =
        std::make_unique<FrameBuffer>(device, 
            static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()), 
            static_cast<uint32_t>(Graphics::Instance().GetScreenHeight())
        );
    ssr_fullscreen_quad_ = std::make_unique<FullscreenQuad>(device);
}

void ScreenSpaceReflectionRenderSystem::Render()
{
    comp_mng_.ForEach<ComponentSsr>([&](uint32_t entity_id, ComponentSsr& ssr)
        {
            ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

            ssr_framebuffer_->Clear(context);
            ssr_framebuffer_->Activate(context, FrameBuffer::usage::color);

            //定数バッファにスクリーンスペースリフレクションのパラメータを転送
                SsrConstants ssr_constants{};
                ssr_constants.distance = ssr.distance;
                ssr_constants.num_steps = ssr.num_steps;
                ssr_constants.max_mip = ssr.max_mip;
                ssr_constants.thickness = ssr.thickness;
                ssr_constants.resolution = ssr.resolution;
                ssr_constants.start_bias = ssr.start_bias;
    
                context->UpdateSubresource(ssr_constant_buffer_.Get(), 0, nullptr, &ssr_constants, 0, 0);
                Graphics::Instance().SetConstantBuffer(static_cast<int>(ConstantBufferSlot::kSsr), 1, ssr_constant_buffer_.GetAddressOf());

            ID3D11ShaderResourceView*srvs[]={
                normal_srv_.Get(),
                depth_srv_.Get(),
                color_srv_.Get(),
                parameter_srv_.Get(),
            };
            ssr_fullscreen_quad_->blit(context, srvs, 0, _countof(srvs), ssr_ps_.Get());

            Graphics::Instance().ClearShaderResourceViews(0, _countof(srvs));
            
            ssr_framebuffer_->Deactivate(context);

            //確認用にコンポーネントに収納
            ssr.ssr_texture = ssr_framebuffer_->GetShaderResourceView(0).Get();
            ssr.normal = normal_srv_.Get();
            ssr.depth = depth_srv_.Get();
            ssr.color = color_srv_.Get();
        });
}