#include "../../headers/post_process/bloom.h"

#include"../../external/imgui/imgui.h"

#include"../../headers/misc.h"
#include"../../headers/resource_manager.h"
#include"../../headers/graphics.h"

Bloom::Bloom(ID3D11Device* device, uint32_t& width, uint32_t& height)
{
    base_width_ = width;
    base_height_ = height;

    DXGI_FORMAT bloom_format = DXGI_FORMAT_R11G11B10_FLOAT;

    for (size_t i = 0; i < downsampled_count; ++i)
    {
        uint32_t mip_width = std::max<uint32_t>(1, width >> i);
        uint32_t mip_height = std::max<uint32_t>(1, height >> i);

        CreateBloomTexture(
            device,
            mip_width,
            mip_height,
            bloom_format,
            bloom_mips_[i]
        );

        CreateBloomTexture(
            device,
            mip_width,
            mip_height,
            bloom_format,
            bloom_temp_[i]
        );
    }

    bloom_extract_cs_ =
        ResourceManager::Instance().LoadComputeShader(
            device,
            L".//resources//shader//bloom_extract_cs.cso"
        );

    bloom_downsample_cs_ =
        ResourceManager::Instance().LoadComputeShader(
            device,
            L".//resources//shader//bloom_downsample_cs.cso"
        );

    bloom_upsample_cs_ =
        ResourceManager::Instance().LoadComputeShader(
            device,
            L".//resources//shader//bloom_upsample_cs.cso"
        );

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(BloomComputeConstants);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    HRESULT hr = device->CreateBuffer(
        &buffer_desc,
        nullptr,
        constant_buffer_.GetAddressOf()
    );

    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}
void Bloom::Make(
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* color_map
)
{
    // 1. Bright extraction
    {
        BloomComputeConstants constants{};
        constants.bloom_extraction_threshold = bloom_constant_.bloom_extraction_threshold;
        constants.bloom_intensity = bloom_constant_.bloom_intensity;
        constants.bloom_soft_knee = bloom_constant_.bloom_soft_knee;
        constants.bloom_radius = bloom_constant_.bloom_radius;
        constants.input_width = base_width_;
        constants.input_height = base_height_;
        constants.output_width = bloom_mips_[0].width;
        constants.output_height = bloom_mips_[0].height;

        context->UpdateSubresource(
            constant_buffer_.Get(),
            0,
            nullptr,
            &constants,
            0,
            0
        );

        ID3D11ShaderResourceView* srvs[] =
        {
            color_map,
            emissive_map_.Get()
        };

        ID3D11UnorderedAccessView* uavs[] =
        {
            bloom_mips_[0].uav.Get()
        };

        context->CSSetShader(bloom_extract_cs_.Get(), nullptr, 0);
        Graphics::Instance().SetConstantBuffer(8, 1, constant_buffer_.GetAddressOf());
        context->CSSetShaderResources(0, 2, srvs);
        context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

        Dispatch2D(context, bloom_mips_[0].width, bloom_mips_[0].height);

        UnbindComputeResources(context);
    }

    // 2. Downsample chain
    for (size_t i = 1; i < downsampled_count; ++i)
    {
        BloomComputeConstants constants{};
        constants.bloom_extraction_threshold = bloom_constant_.bloom_extraction_threshold;
        constants.bloom_intensity = bloom_constant_.bloom_intensity;
        constants.bloom_soft_knee = bloom_constant_.bloom_soft_knee;
        constants.bloom_radius = bloom_constant_.bloom_radius;
        constants.input_width = bloom_mips_[i - 1].width;
        constants.input_height = bloom_mips_[i - 1].height;
        constants.output_width = bloom_mips_[i].width;
        constants.output_height = bloom_mips_[i].height;

        context->UpdateSubresource(
            constant_buffer_.Get(),
            0,
            nullptr,
            &constants,
            0,
            0
        );

        ID3D11ShaderResourceView* srv =
            bloom_mips_[i - 1].srv.Get();

        ID3D11UnorderedAccessView* uav =
            bloom_mips_[i].uav.Get();

        context->CSSetShader(bloom_downsample_cs_.Get(), nullptr, 0);
        //context->CSSetConstantBuffers(8, 1, constant_buffer_.GetAddressOf());
        context->CSSetShaderResources(0, 1, &srv);
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

        Dispatch2D(context, bloom_mips_[i].width, bloom_mips_[i].height);

        UnbindComputeResources(context);
    }

    // 3. Upsample combine
    for (int i = static_cast<int>(downsampled_count) - 2; i >= 0; --i)
    {
        BloomComputeConstants constants{};
        constants.bloom_extraction_threshold = bloom_constant_.bloom_extraction_threshold;
        constants.bloom_intensity = bloom_constant_.bloom_intensity;
        constants.bloom_soft_knee = bloom_constant_.bloom_soft_knee;
        constants.bloom_radius = bloom_constant_.bloom_radius;
        constants.input_width = bloom_mips_[i + 1].width;
        constants.input_height = bloom_mips_[i + 1].height;
        constants.output_width = bloom_mips_[i].width;
        constants.output_height = bloom_mips_[i].height;

        context->UpdateSubresource(
            constant_buffer_.Get(),
            0,
            nullptr,
            &constants,
            0,
            0
        );

        ID3D11ShaderResourceView* srvs[] =
        {
            bloom_mips_[i].srv.Get(),
            bloom_mips_[i + 1].srv.Get()
        };

        ID3D11UnorderedAccessView* uav =
            bloom_temp_[i].uav.Get();

        context->CSSetShader(bloom_upsample_cs_.Get(), nullptr, 0);
        //context->CSSetConstantBuffers(8, 1, constant_buffer_.GetAddressOf());
        
        context->CSSetShaderResources(0, 2, srvs);
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

        Dispatch2D(context, bloom_temp_[i].width, bloom_temp_[i].height);

        UnbindComputeResources(context);

        std::swap(bloom_mips_[i], bloom_temp_[i]);
    }
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Bloom::GetShaderResourceView()
{
    return bloom_mips_[0].srv;
}

void Bloom::DrawImgui()
{
    if (ImGui::TreeNode("bloom"))
    {
        ImGui::SliderFloat(
            "extraction_threshold",
            &bloom_constant_.bloom_extraction_threshold,
            0.f,
            3.f
        );

        ImGui::SliderFloat(
            "intensity",
            &bloom_constant_.bloom_intensity,
            0.f,
            10.f
        );

        ImGui::SliderFloat(
            "soft_knee",
            &bloom_constant_.bloom_soft_knee,
            0.f,
            1.f
        );

        ImGui::SliderFloat(
            "radius",
            &bloom_constant_.bloom_radius,
            0.f,
            2.f
        );

        if (emissive_map_)
        {
            ImGui::Text("emissive");
            ImGui::Image(emissive_map_.Get(), ImVec2(256, 256));
        }

        ImGui::Text("bloom result");
        ImGui::Image(bloom_mips_[0].srv.Get(), ImVec2(256, 256));

        for (size_t i = 0; i < downsampled_count; ++i)
        { 
            ImGui::Text("bloom mip %zu", i);
            ImGui::Image(bloom_mips_[i].srv.Get(), ImVec2(128, 128));
        }

        ImGui::TreePop();
    }
}

void Bloom::CreateBloomTexture(
    ID3D11Device* device,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    BloomTexture& out_texture
)
{
    out_texture.width = width;
    out_texture.height = height;

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = format;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags =
        D3D11_BIND_SHADER_RESOURCE |
        D3D11_BIND_UNORDERED_ACCESS;
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = 0;

    HRESULT hr = device->CreateTexture2D(
        &texture_desc,
        nullptr,
        out_texture.texture.GetAddressOf()
    );

    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(
        out_texture.texture.Get(),
        &srv_desc,
        out_texture.srv.GetAddressOf()
    );

    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = format;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Texture2D.MipSlice = 0;

    hr = device->CreateUnorderedAccessView(
        out_texture.texture.Get(),
        &uav_desc,
        out_texture.uav.GetAddressOf()
    );

    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

void Bloom::Dispatch2D(
    ID3D11DeviceContext* context,
    uint32_t width,
    uint32_t height
)
{
    constexpr uint32_t thread_count_x = 8;
    constexpr uint32_t thread_count_y = 8;

    uint32_t group_x = (width + thread_count_x - 1) / thread_count_x;
    uint32_t group_y = (height + thread_count_y - 1) / thread_count_y;

    context->Dispatch(group_x, group_y, 1);
}

void Bloom::UnbindComputeResources(ID3D11DeviceContext* context)
{
    ID3D11ShaderResourceView* null_srvs[8]{};
    ID3D11UnorderedAccessView* null_uavs[8]{};

    context->CSSetShaderResources(0, 8, null_srvs);
    context->CSSetUnorderedAccessViews(0, 8, null_uavs, nullptr);
    context->CSSetShader(nullptr, nullptr, 0);
}
