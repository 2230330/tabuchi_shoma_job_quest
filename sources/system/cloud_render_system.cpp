#include"../../headers/system/cloud_render_system.h"

#include<algorithm>
#include<iostream>
#include<DirectXTex.h>
#include<DDSTextureLoader.h>
#include<filesystem>

#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
#include"../../headers/constant_buffer_slot.h"
#include"../../headers/misc.h"

CloudRenderSystem::CloudRenderSystem(ComponentManager& comp_mng,RenderPass render_pass)
    :comp_mng_(comp_mng)
    ,IRenderSystem(render_pass)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();

    //定数バッファ生成
    {
        HRESULT hr{ S_OK };

        D3D11_BUFFER_DESC cb_desc{};
        cb_desc.Usage = D3D11_USAGE_DEFAULT;
        cb_desc.ByteWidth = (sizeof(CloudRayMarchingConstants) + 15) / 16 * 16;//16バイト境界に丸め
        cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb_desc.CPUAccessFlags = 0;
        cb_desc.MiscFlags = 0;
        hr = device->CreateBuffer(&cb_desc, nullptr, cloud_ray_marching_constant_buffer_.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    }

    //コンピュートシェーダーで使うノイズテクスチャの作成
    CreateNoiseTextures(device);

    //ノイズテクスチャの読み込み
    {
        HRESULT hr{ S_OK };
        
        const wchar_t* low_freq_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\low_freq_perlin_worley.dds";
        _ASSERT_EXPR(std::filesystem::exists(low_freq_noise_tex_path), "ファイルが存在しません");
        {
            DirectX::TexMetadata metadata{};
            DirectX::ScratchImage image{};
            HRESULT hr = DirectX::LoadFromDDSFile(
                low_freq_noise_tex_path,
                DirectX::DDS_FLAGS_NONE,
                &metadata,
                image);
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

            const DirectX::Image* images = image.GetImages();
            size_t nimages = image.GetImageCount();
            hr=DirectX::CreateShaderResourceViewEx(
                device,
                images,
                nimages,
                metadata,
                D3D11_USAGE_DEFAULT, 
                D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET, 
                0, 0, DirectX::CREATETEX_FLAGS::CREATETEX_DEFAULT, 
                low_freq_perlin_worley_srv_.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }
        const wchar_t* high_freq_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\high_freq_worley.dds";
        _ASSERT_EXPR(std::filesystem::exists(high_freq_noise_tex_path), "ファイルが存在しません");
        {
            DirectX::TexMetadata metadata{};
            DirectX::ScratchImage image{};
            HRESULT hr = DirectX::LoadFromDDSFile(
                high_freq_noise_tex_path,
                DirectX::DDS_FLAGS_NONE,
                &metadata,
                image);
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
            const DirectX::Image* images = image.GetImages();
            size_t nimages = image.GetImageCount();
            hr = DirectX::CreateShaderResourceViewEx(
                device,
                images,
                nimages,
                metadata,
                D3D11_USAGE_DEFAULT,
                D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET,
                0, 0, DirectX::CREATETEX_FLAGS::CREATETEX_DEFAULT,
                high_freq_perlin_worley_srv_.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }

        const wchar_t* weather_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\weather.bmp";
        weather_map_srv_ = ResourceManager::Instance().LoadTextureFromFile(device, weather_noise_tex_path);
        const wchar_t* curl_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\curl_noise.png";
        curl_noise_srv_ = ResourceManager::Instance().LoadTextureFromFile(device, curl_noise_tex_path);
    }

    // InputLayoutとシェーダーの読み込み（仮）
    D3D11_INPUT_ELEMENT_DESC input_element_desc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    //シェーダーの設定
    cloud_ps_ = ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\cloud_dome_ps.cso");


    //フルスクリーンクワッドの作成
    fullscreen_quad_ = std::make_unique<FullscreenQuad>(device);
}
void CloudRenderSystem::Render()
{
    comp_mng_.ForEach<ComponentCloudDome>([&](uint32_t entity_id, ComponentCloudDome& cloud)
        {
            ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

            RenderState render_state(Graphics::Instance().GetDevice());
            // 深度・カリング設定（球の内側を描画）
            render_state.GetDepthStencilState(DepthState::no_test_no_write);
            render_state.GetRasterizerState(RasterizerState::solid_cull_none);
            render_state.GetBlendState(BlendState::transparency);

            //定数バッファの更新
            UpdateConstants(cloud);
            context->UpdateSubresource(
                cloud_ray_marching_constant_buffer_.Get(), 0, nullptr, &cloud_ray_marching_constant_, 0, 0);
            Graphics::Instance().SetConstantBuffer(
                static_cast<int>(ConstantBufferSlot::kCloudDome),
                1,
                cloud_ray_marching_constant_buffer_.GetAddressOf());

            //// テクスチャ
            ID3D11ShaderResourceView* srvs[] = {
                low_freq_perlin_worley_srv_.Get(),
                high_freq_perlin_worley_srv_.Get(),
                weather_map_srv_.Get(),
                curl_noise_srv_.Get()
            };
            Graphics::Instance().SetShaderResource(0, _countof(srvs), srvs);
            // 描画呼び出し
            fullscreen_quad_->blit(context, srvs, 0, 4,cloud_ps_.Get());


        });
}
//初期に1度だけ呼び出し、ノイズマップを作成
void CloudRenderSystem::CreateNoiseTextures(ID3D11Device* device)
{

    HRESULT hr{ S_OK };
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

    context->Flush();

    const wchar_t* low_freq_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\low_freq_perlin_worley.dds";
    {
        // 3Dテクスチャ作成（MipMap対応）
        Microsoft::WRL::ComPtr<ID3D11Texture3D> low_freq_tex;
        D3D11_TEXTURE3D_DESC desc{};
        desc.Width = low_freq_perlin_worley_dimensions;
        desc.Height = low_freq_perlin_worley_dimensions;
        desc.Depth = low_freq_perlin_worley_dimensions;
        desc.MipLevels = 0; // 自動生成
        desc.Format = format;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS; // MipMap生成可能に
        hr = device->CreateTexture3D(&desc, nullptr, low_freq_tex.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        // SRV作成（MipMap対応）
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> low_freq_srv;
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        srvDesc.Texture3D.MipLevels = -1; // 全MipMap使用
        srvDesc.Texture3D.MostDetailedMip = 0;
        hr = device->CreateShaderResourceView(low_freq_tex.Get(), &srvDesc, low_freq_srv.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        // UAV作成
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> low_freq_uav;
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = desc.Format;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
        uavDesc.Texture3D.MipSlice = 0;
        uavDesc.Texture3D.FirstWSlice = 0;
        uavDesc.Texture3D.WSize = -1;
        hr = device->CreateUnorderedAccessView(low_freq_tex.Get(), &uavDesc, low_freq_uav.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        // コンピュートシェーダーでノイズ生成
        auto cs = ResourceManager::Instance().LoadComputeShader(device, L".\\resources\\shader\\low_freq_perlin_worley_cs.cso");
        context->CSSetUnorderedAccessViews(0, 1, low_freq_uav.GetAddressOf(), nullptr);
        context->CSSetShader(cs.Get(), nullptr, 0);
        UINT threadGroupCount = (low_freq_perlin_worley_dimensions + low_freq_perlin_worley_numthreads - 1) / low_freq_perlin_worley_numthreads;
        context->Dispatch(threadGroupCount, threadGroupCount, threadGroupCount);

        // GPU完了待ち（同期）
        D3D11_QUERY_DESC queryDesc{};
        queryDesc.Query = D3D11_QUERY_EVENT;
        Microsoft::WRL::ComPtr<ID3D11Query> query;
        device->CreateQuery(&queryDesc, &query);
        context->End(query.Get());
        while (S_OK != context->GetData(query.Get(), nullptr, 0, 0)) { Sleep(1); }
        // UAV解除
        ID3D11UnorderedAccessView* nullUAV = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
        context->CSSetShader(nullptr, nullptr, 0);

        // MipMap生成
        context->GenerateMips(low_freq_srv.Get());


        // DDS保存
        DirectX::ScratchImage image;
        hr = DirectX::CaptureTexture(device, context, low_freq_tex.Get(), image);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        hr = DirectX::SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
            DirectX::DDS_FLAGS_NONE, low_freq_noise_tex_path);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    const wchar_t* high_freq_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\high_freq_worley.dds";
    {
        //空のテクスチャを作成
        Microsoft::WRL::ComPtr<ID3D11Texture3D> high_freq_worley_texture3d;
        D3D11_TEXTURE3D_DESC texture3d_desc{};
        texture3d_desc.Width = high_freq_worley_dimensions;
        texture3d_desc.Height = high_freq_worley_dimensions;
        texture3d_desc.Depth = high_freq_worley_dimensions;
        texture3d_desc.MipLevels = 1;
        texture3d_desc.MiscFlags = 0;
        texture3d_desc.Format = format;
        texture3d_desc.Usage = D3D11_USAGE_DEFAULT;
        texture3d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
        texture3d_desc.CPUAccessFlags = 0;
        hr = device->CreateTexture3D(&texture3d_desc, NULL, high_freq_worley_texture3d.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        //シェーダーリソースビューを作成
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> high_freq_worley_srv;
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format = texture3d_desc.Format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        srv_desc.Texture3D.MipLevels = 1;
        srv_desc.Texture3D.MostDetailedMip = 0;
        hr = device->CreateShaderResourceView(
            high_freq_worley_texture3d.Get(), &srv_desc, high_freq_worley_srv.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        //アンオーダードアクセスビューを作成
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>high_freq_worley_unordered_access_view;
        D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_access_view_desc = {};
        unordered_access_view_desc.Format = texture3d_desc.Format;
        unordered_access_view_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
        unordered_access_view_desc.Texture3D.MipSlice = 0;
        unordered_access_view_desc.Texture3D.FirstWSlice = 0;
        unordered_access_view_desc.Texture3D.WSize = -1;
        hr = device->CreateUnorderedAccessView(high_freq_worley_texture3d.Get(), &unordered_access_view_desc, high_freq_worley_unordered_access_view.GetAddressOf());   
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        //コンピュートシェーダで3Dテクスチャ内部にノイズを書き込む
        Microsoft::WRL::ComPtr<ID3D11ComputeShader> high_freq_worley_cs;
        high_freq_worley_cs = ResourceManager::Instance().LoadComputeShader(
            device, L"./resources/shader/high_freq_worley_cs.cso"
        );
        context->CSSetUnorderedAccessViews(0, 1, high_freq_worley_unordered_access_view.GetAddressOf(), nullptr);
        context->CSSetShader(high_freq_worley_cs.Get(), nullptr, 0);
        UINT thread_group_count =
            (high_freq_worley_dimensions + high_freq_worley_numthreads - 1)/ high_freq_worley_numthreads;
        context->Dispatch(thread_group_count, thread_group_count, thread_group_count);

        ID3D11UnorderedAccessView* null_unordered_access_view = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &null_unordered_access_view,nullptr);
        context->CSSetShader(nullptr, nullptr,0);
        //みっぷマップを自動生成
        //context->GenerateMips(high_freq_worley_srv.Get());
        
        //GPU命令をキック
        // GPU完了待ち（同期）
        D3D11_QUERY_DESC queryDesc{};
        queryDesc.Query = D3D11_QUERY_EVENT;
        Microsoft::WRL::ComPtr<ID3D11Query> query;
        device->CreateQuery(&queryDesc, &query);
        context->End(query.Get());
        while (S_OK != context->GetData(query.Get(), nullptr, 0, 0)) { Sleep(1); }

        //DDSファイルとして書き出し
        DirectX::ScratchImage image;
        hr = DirectX::CaptureTexture(device, context, high_freq_worley_texture3d.Get(), image);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        hr = DirectX::SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(), 
            DirectX::DDS_FLAGS_NONE, high_freq_noise_tex_path);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    }
}

void CloudRenderSystem::UpdateConstants(const ComponentCloudDome& cloud)
{
    // --- 基本情報 ---
    cloud_ray_marching_constant_.wind_direction = cloud.wind_direction;
    cloud_ray_marching_constant_.cloud_altitudes_min_max = cloud.cloud_altitudes_min_max;

    cloud_ray_marching_constant_.wind_speed = cloud.wind_speed;

    // --- 雲の密度・形状 ---
    cloud_ray_marching_constant_.density_scale = cloud.density_scale;
    cloud_ray_marching_constant_.cloud_coverage_scale = cloud.cloud_coverage_scale;
    cloud_ray_marching_constant_.rain_cloud_absorption_scale = cloud.rain_cloud_absorption_scale;
    cloud_ray_marching_constant_.cloud_type_scale = cloud.cloud_type_scale;

    // --- 球体大気モデル ---
    cloud_ray_marching_constant_.earth_radius = cloud.earth_radius;
    cloud_ray_marching_constant_.horizon_distance_scale = cloud.horizon_distance_scale;

    // --- ノイズサンプリング ---
    cloud_ray_marching_constant_.low_frequency_perlin_worley_sampling_scale =
        cloud.low_frequency_perlin_worley_sampling_scale;
    cloud_ray_marching_constant_.high_frequency_worley_sampling_scale =
        cloud.high_frequency_worley_sampling_scale;

    cloud_ray_marching_constant_.cloud_density_long_distance_scale =
        cloud.cloud_density_long_distance_scale;

    cloud_ray_marching_constant_.enable_powdered_sugar_efffect =
        cloud.enable_powdered_sugar_efffect;

    // --- レイマーチング設定 ---
    cloud_ray_marching_constant_.ray_marching_steps = cloud.ray_marching_steps;
    cloud_ray_marching_constant_.auto_ray_marching_steps = cloud.auto_ray_marching_steps;

}
