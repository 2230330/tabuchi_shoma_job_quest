#include"../../headers/system/render_screen_space_reflection_system.h"

#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
ScreenSpaceReflectionRenderSystem::ScreenSpaceReflectionRenderSystem(ComponentManager& comp_mng, RenderPass render_pass)
    :comp_mng_(comp_mng)
    ,IRenderSystem(render_pass)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
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

            ID3D11ShaderResourceView*srvs[]={
                normal_srv_.Get(),
                depth_srv_.Get(),
                color_srv_.Get(),
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