#include"../../headers/system/render_sky_system.h"

#include <DirectXTex.h>
#include<DirectXMath.h>

#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
#include"../../headers/render_state.h"
#include"../../headers/constant_buffer_slot.h"
#include"../../headers/fullscreen_quad.h"
#include"../../headers/framebuffer.h"
#include"../../headers/misc.h"
#include"../../headers/component/component_manager.h"

RenderSkySystem::RenderSkySystem(ComponentManager& comp_mng, RenderPass render_pass)
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

    InitializeGpuTimer(device);
}


void RenderSkySystem::Render()
{
    sky_flag_ = false;
    comp_mng_.ForEach<ComponentSkyAtmosphere>([&](uint32_t entity_id, ComponentSkyAtmosphere& sky_atmosphere) {
        {
            sky_flag_ = true;

            ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

            int read_index = (write_index_ + QUERY_BUFFER_COUNT-2) % QUERY_BUFFER_COUNT;
            UpdateGpuTimer(read_index);
            sky_atmosphere.gpu_time_ms = gpu_time_ms_;

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

            //GPU負荷計測開始
            BeginGpuFrame(write_index_);

            // 描画呼び出し
            fullscreen_quad_->blit(context, nullptr, 0, 0, sky_ps_.Get());

            //GPU負荷計測終了
            EndGpuFrame(write_index_);
        }

        });

    write_index_ = (write_index_ + 1) % QUERY_BUFFER_COUNT;
}

void RenderSkySystem::InitializeGpuTimer(ID3D11Device* device)
{
    HRESULT hr{ S_OK };

    D3D11_QUERY_DESC desc = {};
    for (int i = 0; i < QUERY_BUFFER_COUNT; i++)
    {

        desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        hr = device->CreateQuery(&desc, dis_joint_query_[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        desc.Query = D3D11_QUERY_TIMESTAMP;
        hr = device->CreateQuery(&desc, time_stamp_start_query_[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hr = device->CreateQuery(&desc, time_stamp_end_query_[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    }
}

void RenderSkySystem::BeginGpuFrame(int write_index)
{
    ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();
    context->Begin(dis_joint_query_[write_index].Get());
    context->End(time_stamp_start_query_[write_index].Get());
}

void RenderSkySystem::UpdateGpuTimer(int read_index)
{
    ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();
    HRESULT hr{ S_OK };

    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;

    hr = context->GetData(
        dis_joint_query_[read_index].Get(),
        &disjoint,
        sizeof(disjoint),
        0);
    if (hr != S_OK)
    {
        return;
    }

    UINT64 start;
    UINT64 end;

    hr = context->GetData(
        time_stamp_start_query_[read_index].Get(),
        &start,
        sizeof(start),
        0);
    if (hr != S_OK)
    {
        return;
    }

    hr = context->GetData(
        time_stamp_end_query_[read_index].Get(),
        &end,
        sizeof(end),
        0);
    if (hr != S_OK)
    {
        return;
    }
    if (!disjoint.Disjoint)
    {
        gpu_time_ms_ =
            double(end - start) *
            1000.0 /
            double(disjoint.Frequency);
    }

}

void RenderSkySystem::EndGpuFrame(int write_index)
{
    ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();
    context->End(time_stamp_end_query_[write_index].Get());
    context->End(dis_joint_query_[write_index].Get());
}

