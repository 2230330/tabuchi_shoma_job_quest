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
    const wchar_t* dither_noise_path = L".\\resources\\sprite\\blue_noise.dds";
    _ASSERT_EXPR(std::filesystem::exists(dither_noise_path), "ファイルが存在しません");
    {
        dither_noise_map_ = ResourceManager::Instance().LoadTextureFromFile(device, dither_noise_path);
    }
    fullscreen_quad_ = std::make_unique<FullscreenQuad>(Graphics::Instance().GetDevice());

    InitializeGpuTimer(device);
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
                fog_constant_.fog_max_height = fog.fog_height_max;
                fog_constant_.fog_intensity = fog.fog_intensity;
                fog_constant_.fog_color = fog.fog_color;

                ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

                int read_index = (write_index_ + QUERY_BUFFER_COUNT - 15) % QUERY_BUFFER_COUNT;
                UpdateGpuTimer(read_index);
                fog.gpu_time_ms = static_cast<float>(gpu_time_ms_);


                context->UpdateSubresource(
                    fog_constant_buffer_.Get(), 0, nullptr, &fog_constant_, 0, 0);
                Graphics::Instance().SetConstantBuffer(ConstantBufferSlot::kFog, 1,
                    fog_constant_buffer_.GetAddressOf());

                ID3D11ShaderResourceView* srvs[] =
                {
                    depth_map_.Get(),
                    noise_map_.Get(),
                };

                //GPU負荷計測開始 
                BeginGpuFrame(write_index_);

                //描画呼び出し
                fullscreen_quad_->blit(context, srvs, 0, _countof(srvs),fog_ps_.Get());
                
                //GPU負荷計測終了
                EndGpuFrame(write_index_);

        });
    write_index_ = (write_index_ + 1) % QUERY_BUFFER_COUNT;
}

void RenderFogSystem::SetObjectDepthView(ID3D11ShaderResourceView* depth_map)
{
    depth_map_ = depth_map;
}

void RenderFogSystem::SetObjectResolution(float width, float height)
{
    fog_constant_.object_resolution_width = width;
    fog_constant_.object_resolution_height = height;
}

void RenderFogSystem::InitializeGpuTimer(ID3D11Device* device)
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

void RenderFogSystem::BeginGpuFrame(int write_index)
{
    ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();
    context->Begin(dis_joint_query_[write_index].Get());
    context->End(time_stamp_start_query_[write_index].Get());
}

void RenderFogSystem::UpdateGpuTimer(int read_index)
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

        OutputDebugStringA("DISJOINT NOT READY\n");
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

void RenderFogSystem::EndGpuFrame(int write_index)
{
    ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();
    context->End(time_stamp_end_query_[write_index].Get());
    context->End(dis_joint_query_[write_index].Get());
}

