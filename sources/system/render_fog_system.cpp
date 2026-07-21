#include"../../headers/system/render_fog_system.h"

#include<filesystem>

#include"../../headers/graphics.h"
#include"../../headers/component/component_manager.h"
#include"../../headers/fullscreen_quad.h"
#include"../../headers/framebuffer.h"
#include"../../headers/misc.h"
#include"../../headers/constant_buffer_slot.h"
#include"../../headers/resource_manager.h"

RenderFogSystem::RenderFogSystem(ComponentManager& comp_mng, RenderPass render_pass)
    :comp_mng_(comp_mng)
    , IRenderSystem(render_pass)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
    //バッファ生成
    {
        HRESULT hr{ S_OK };

        //定数バッファ生成
        {
            D3D11_BUFFER_DESC cb_desc{};
            cb_desc.Usage = D3D11_USAGE_DEFAULT;
            cb_desc.ByteWidth = (sizeof(FogConstants) + 15) / 16 * 16;
            cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            hr = device->CreateBuffer(&cb_desc, nullptr, fog_constant_buffer_.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }
    }

    fog_ps_ = ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\fog_ps.cso");
    const wchar_t* low_freq_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\low_freq_perlin_worley.dds";
    _ASSERT_EXPR(std::filesystem::exists(low_freq_noise_tex_path), "ファイルが存在しません");
    {
        noise_map_ = ResourceManager::Instance().LoadTextureFromFile(device, low_freq_noise_tex_path);
    }
    fullscreen_quad_ = std::make_unique<FullscreenQuad>(Graphics::Instance().GetDevice());
    frame_buffer_ = std::make_unique<FrameBuffer>(Graphics::Instance().GetDevice(),
        static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()),
        static_cast<uint32_t>(Graphics::Instance().GetScreenHeight()));
}

void RenderFogSystem::Render()
{
    comp_mng_.ForEach<ComponentFog>
        ([&](uint32_t entity_id, ComponentFog& fog) 
            {
                fog_constant_.fog_steps = fog.fog_steps;
                fog_constant_.fog_max_distance = fog.fog_max_distance;
                fog_constant_.fog_density = fog.fog_density;
                fog_constant_.noise_scale = fog.noise_scale;

                ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

                //frame_buffer_->Clear(context);
                //frame_buffer_->Activate(context);

                context->UpdateSubresource(
                    fog_constant_buffer_.Get(), 0, nullptr, &fog_constant_, 0, 0);
                Graphics::Instance().SetConstantBuffer(ConstantBufferSlot::kFog, 1,
                    fog_constant_buffer_.GetAddressOf());

                ID3D11ShaderResourceView* srvs[] =
                {
                    depth_map_.Get(),
                    noise_map_.Get()
                };

                fullscreen_quad_->blit(context, srvs, 0, _countof(srvs),fog_ps_.Get());
                
                //frame_buffer_->Deactivate(context);
        });
}

void RenderFogSystem::SetObjectDepthView(ID3D11ShaderResourceView* depth_map)
{
    depth_map_ = depth_map;
}

