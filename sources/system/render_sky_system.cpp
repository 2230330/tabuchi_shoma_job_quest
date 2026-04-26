#include"../../headers/system/render_sky_system.h"

#include <DirectXTex.h>

#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
#include"../../headers/render_state.h"
#include"../../headers/misc.h"
#include"../../headers/constant_buffer_slot.h"

SkyRenderSystem::SkyRenderSystem(ComponentManager& comp_mng, RenderPass render_pass)
    :comp_mng_(comp_mng)
    , IRenderSystem(render_pass)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();

    //Texture2D/SRV
    {

    }

    //バッファ生成
    {
        HRESULT hr{ S_OK };

        //定数バッファ生成
        {
            D3D11_BUFFER_DESC cb_desc{};
            cb_desc.Usage = D3D11_USAGE_DEFAULT;
            cb_desc.ByteWidth = (sizeof(SkyAtmosphereCB)+15)/16*16;
            cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            hr = device->CreateBuffer(&cb_desc, nullptr, rayleigh_constant_buffer_.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        }
    }

    sky_ps_ = ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\sky_atmosphere_ps.cso");

    //フルスクリーンクワッド
    fullscreen_quad_ = std::make_unique<FullscreenQuad>(device);
}


void SkyRenderSystem::Render()
{
    sky_flag_ = false;
    comp_mng_.ForEach<ComponentSkyAtmosphere>([&](uint32_t entity_id, ComponentSkyAtmosphere sky_atmosphere) {
        {
            sky_flag_ = true;

            ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

            RenderState render_state(Graphics::Instance().GetDevice());
            // 深度・カリング設定（球の内側を描画）
            render_state.GetDepthStencilState(DepthState::no_test_no_write);
            render_state.GetRasterizerState(RasterizerState::solid_cull_none);
            render_state.GetBlendState(BlendState::transparency);

            // シェーダー設定
            context->PSSetShader(sky_ps_.Get(), nullptr, 0);

            //定数バッファの設定
            sky_atmosphere_constant.rayleigh_scale_height = sky_atmosphere.rayleigh_scale_height;
            sky_atmosphere_constant.mie_scale_height = sky_atmosphere.mie_scale_height;
            sky_atmosphere_constant.ozone_scale_half_width = sky_atmosphere.ozone_scale_half_width;
            sky_atmosphere_constant.ozone_center_height = sky_atmosphere.ozone_center_height;
            sky_atmosphere_constant.atmosphere_height = sky_atmosphere.atmosphere_height;
            sky_atmosphere_constant.sun_distance = sky_atmosphere.sun_distance;
            sky_atmosphere_constant.earth_height = sky_atmosphere.earth_height;
            sky_atmosphere_constant.max_sample = sky_atmosphere.max_sample;
            sky_atmosphere_constant.height = comp_mng_.TryGetByEntity<ComponentPosition>(entity_id)->value.y * 1e3f;
            SkyAtmosphereCB data = sky_atmosphere_constant;
            context->UpdateSubresource(rayleigh_constant_buffer_.Get(), 0, 0, &data, 0, 0);
            Graphics::Instance().SetConstantBuffer(
                static_cast<int>(ConstantBufferSlot::kSkyAtmosphere),
                1,
                rayleigh_constant_buffer_.GetAddressOf());

            // 描画呼び出し
            fullscreen_quad_->blit(context, nullptr, 0, 0, sky_ps_.Get());

        }

        });
}

