
#include "../../headers/system/ibl_manager.h"

#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <DirectXMath.h>
#include <algorithm>
#include <vector>
#include<filesystem>

#include"../../headers/graphics.h"
#include "../../headers/resource_manager.h"
#include"../../headers/misc.h"

#pragma comment(lib, "d3dcompiler.lib")


// ---- ヘルパ ----
UINT IBLManager::CalcMipCount(UINT size) {
    UINT mips = 1;
    while (size > 1) { size >>= 1; ++mips; }
    return mips;
}
HRESULT IBLManager::LoadCSO(const wchar_t* path, Microsoft::WRL::ComPtr<ID3DBlob>& blob) {
    return D3DReadFileToBlob(path, blob.GetAddressOf());
}

// ============================
// IBLManager 実装
// ============================
void IBLManager::Initialize(ID3D11Device* dev)
{
    dev_ = dev;
    dev_->GetImmediateContext(ctx_.GetAddressOf());

    // --- サンプラ（linear clamp） ---
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        dev_->CreateSamplerState(&sd, samp_linear_clamp_.GetAddressOf());
    }

    // --- BRDF LUT（RG16F 2D） + UAV/SRV ---
    {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = kBrdfLutSize;
        td.Height = kBrdfLutSize;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R16G16_FLOAT;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

        dev_->CreateTexture2D(&td, nullptr, brdf_lut_tex_.GetAddressOf());

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavd{};
        uavd.Format = td.Format;
        uavd.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavd.Texture2D.MipSlice = 0;
        dev_->CreateUnorderedAccessView(brdf_lut_tex_.Get(), &uavd, brdf_lut_uav_.GetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = td.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = 1;
        dev_->CreateShaderResourceView(brdf_lut_tex_.Get(), &sd, brdf_lut_srv_.GetAddressOf());

        srv_brdf_lut_ = brdf_lut_srv_;
    }

    // --- BRDF LUT CS をロード＆生成（起動時1回） ---
    {
        auto cs = ResourceManager::Instance().LoadComputeShader(dev_.Get(), L".\\resources\\shader\\ibl_brdf_lut_cs.cso");
        cs_brdf_lut_ = cs;

        ID3D11UnorderedAccessView* uavs[1] = { brdf_lut_uav_.Get() };
        ctx_->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
        ctx_->CSSetShader(cs_brdf_lut_.Get(), nullptr, 0);
        ctx_->Dispatch(kBrdfLutSize / 8, kBrdfLutSize / 8, 1);
        ID3D11UnorderedAccessView* nullUAV[1] = { nullptr };
        ctx_->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);
        ctx_->CSSetShader(nullptr, nullptr, 0);
    }

    // --- LatLong→Cube 変換 CS（ロード） ---
    {
        cs_latlong_to_cube_ = 
            ResourceManager::Instance().LoadComputeShader(dev_.Get(), L".\\resources\\shader\\latlong_to_cubemap_cs.cso");
    }

    // --- Prefilter 用 VS/PS と b0 ---
    {
        // VS は FullscreenQuad 用（SV_VertexID、入力レイアウト無し）
        ibl_screen_vs_ = 
            ResourceManager::Instance().LoadVertexShader(dev_.Get(), L".\\resources\\shader\\ibl_screen_vs.cso", nullptr, nullptr, 0);

        ps_prefilter_ = 
            ResourceManager::Instance().LoadPixelShader(dev_.Get(), L".\\resources\\shader\\ibl_prefilter_ps.cso");

        //geometry shader
        // GeometryShader(.cso) を読み込み
        Microsoft::WRL::ComPtr<ID3DBlob> gsBlob;
        if (SUCCEEDED(D3DReadFileToBlob(L".\\resources\\shader\\ibl_screen_gs.cso", gsBlob.GetAddressOf())))
        {
            dev_->CreateGeometryShader(gsBlob->GetBufferPointer(), gsBlob->GetBufferSize(), nullptr, latlong_to_cube_gs_.GetAddressOf());
        }

        // LatLong->Cubemap用PS
        latlong_to_cube_ps_ = 
            ResourceManager::Instance().LoadPixelShader(dev_.Get(), L".\\resources\\shader\\latlong_to_cubemap_ps.cso");
        sky_cube_ps_ = 
            ResourceManager::Instance().LoadPixelShader(dev_.Get(), L".\\resources\\shader\\ibl_sky_atmosphere_ps.cso");
        cloud_cube_ps_ =
            ResourceManager::Instance().LoadPixelShader(dev_.Get(), L".\\resources\\shader\\ibl_volumetric_cloud_ps.cso");

        D3D11_BUFFER_DESC cbd{};
        cbd.ByteWidth = sizeof(PrefilterCB);
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        dev_->CreateBuffer(&cbd, nullptr, cb_prefilter_.GetAddressOf());

        //sky_cube 用 b0
        cbd.ByteWidth = sizeof(SkyCubeCB);
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        dev_->CreateBuffer(&cbd, nullptr, cb_sky_cube_.GetAddressOf());
    }

    // --- SH 定数バッファ（b2） ---
    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(SH9Constants);
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        dev_->CreateBuffer(&bd, nullptr, cb_sh_.GetAddressOf());

        SH9Constants init{};
        for (int i = 0; i < 9; ++i) init.c[i] = DirectX::XMFLOAT3(0, 0, 0);
        init.c[0] = DirectX::XMFLOAT3(0.03f, 0.03f, 0.03f);
        ctx_->UpdateSubresource(cb_sh_.Get(), 0, nullptr, &init, 0, 0);
    }

    // --- Specular Prefilter 出力キューブ（mip 付き） ---
    {
        const UINT mipCount = CalcMipCount(kPrefilterSize);

        D3D11_TEXTURE2D_DESC td{};
        td.Width = kPrefilterSize; td.Height = kPrefilterSize;
        td.MipLevels = mipCount; td.ArraySize = kCubeFaces;
        td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        dev_->CreateTexture2D(&td, nullptr, prefilter_tex_.GetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = td.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        sd.TextureCube.MostDetailedMip = 0;
        sd.TextureCube.MipLevels = mipCount;
        dev_->CreateShaderResourceView(prefilter_tex_.Get(), &sd, prefilter_srv_.GetAddressOf());

        srv_pref_env_ = prefilter_srv_;

        prefilter_rtvs_.clear();
        prefilter_rtvs_.reserve(kCubeFaces * mipCount);
        for (UINT face = 0; face < kCubeFaces; ++face) {
            for (UINT mip = 0; mip < mipCount; ++mip) {
                D3D11_RENDER_TARGET_VIEW_DESC rd{};
                rd.Format = td.Format;
                rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rd.Texture2DArray.ArraySize = 1;
                rd.Texture2DArray.FirstArraySlice = face;
                rd.Texture2DArray.MipSlice = mip;
                Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
                dev_->CreateRenderTargetView(prefilter_tex_.Get(), &rd, rtv.GetAddressOf());
                prefilter_rtvs_.emplace_back(std::move(rtv));
            }
        }
    }

    // --- SkyCube（未フィルタ：背景表示/IBL入力） ---
    {
        const UINT mipCount = CalcMipCount(kSkyCubeSize);

        D3D11_TEXTURE2D_DESC td{};
        td.Width = kSkyCubeSize; td.Height = kSkyCubeSize;
        td.MipLevels = 1; // まずは mip0 のみ。必要なら自前で生成
        td.ArraySize = kCubeFaces;
        td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_RENDER_TARGET;
        td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        dev_->CreateTexture2D(&td, nullptr, sky_cube_tex_.GetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = td.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        sd.TextureCube.MostDetailedMip = 0;
        sd.TextureCube.MipLevels = 1;
        dev_->CreateShaderResourceView(sky_cube_tex_.Get(), &sd, sky_cube_srv_.GetAddressOf());

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavd{};
        uavd.Format = td.Format;
        uavd.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
        uavd.Texture2DArray.MipSlice = 0;
        uavd.Texture2DArray.FirstArraySlice = 0;
        uavd.Texture2DArray.ArraySize = kCubeFaces;
        dev_->CreateUnorderedAccessView(sky_cube_tex_.Get(), &uavd, sky_cube_uav_.GetAddressOf());

        D3D11_RENDER_TARGET_VIEW_DESC rd{};
        rd.Format = td.Format;
        rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rd.Texture2DArray.MipSlice = 0;
        rd.Texture2DArray.FirstArraySlice = 0;
        rd.Texture2DArray.ArraySize = kCubeFaces;
        dev_->CreateRenderTargetView(sky_cube_tex_.Get(), &rd, cube_rtv_all_.GetAddressOf());

        sky_cube_rtvs_.clear();
        sky_cube_rtvs_.reserve(kCubeFaces);
        for (UINT face = 0; face < kCubeFaces; ++face) {
                D3D11_RENDER_TARGET_VIEW_DESC rd{};
                rd.Format = td.Format;
                rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rd.Texture2DArray.ArraySize = 1;
                rd.Texture2DArray.FirstArraySlice = face;
                rd.Texture2DArray.MipSlice = 0;
                Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
                dev_->CreateRenderTargetView(sky_cube_tex_.Get(), &rd, rtv.GetAddressOf());
                sky_cube_rtvs_.emplace_back(std::move(rtv));
            
        }

    }
    //雲ボックス
    {
        const UINT mipCount = CalcMipCount(kSkyCubeSize);

        D3D11_TEXTURE2D_DESC td{};
        td.Width = kSkyCubeSize; td.Height = kSkyCubeSize;
        td.MipLevels = 1; // まずは mip0 のみ。必要なら自前で生成
        td.ArraySize = kCubeFaces;
        td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
        td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        dev_->CreateTexture2D(&td, nullptr, cloud_cube_tex_.GetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = td.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        sd.TextureCube.MostDetailedMip = 0;
        sd.TextureCube.MipLevels = 1;
        dev_->CreateShaderResourceView(cloud_cube_tex_.Get(), &sd, cloud_cube_srv_.GetAddressOf());

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavd{};
        uavd.Format = td.Format;
        uavd.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
        uavd.Texture2DArray.MipSlice = 0;
        uavd.Texture2DArray.FirstArraySlice = 0;
        uavd.Texture2DArray.ArraySize = kCubeFaces;
        dev_->CreateUnorderedAccessView(cloud_cube_tex_.Get(), &uavd, cloud_cube_uav_.GetAddressOf());

        //D3D11_RENDER_TARGET_VIEW_DESC rd{};
        //rd.Format = td.Format;
        //rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        //rd.Texture2DArray.MipSlice = 0;
        //rd.Texture2DArray.FirstArraySlice = 0;
        //rd.Texture2DArray.ArraySize = kCubeFaces;
        //dev_->CreateRenderTargetView(cloud_cube_tex_.Get(), &rd, cube_rtv_all_.GetAddressOf());

        cloud_cube_rtvs_.clear();
        cloud_cube_rtvs_.reserve(kCubeFaces);
        for (UINT face = 0; face < kCubeFaces; ++face) {
            D3D11_RENDER_TARGET_VIEW_DESC rd{};
            rd.Format = td.Format;
            rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rd.Texture2DArray.ArraySize = 1;
            rd.Texture2DArray.FirstArraySlice = face;
            rd.Texture2DArray.MipSlice = 0;
            Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
            dev_->CreateRenderTargetView(cloud_cube_tex_.Get(), &rd, rtv.GetAddressOf());
            cloud_cube_rtvs_.emplace_back(std::move(rtv));

        }
    }

    //雲用ノイズテクスチャ
    {
        HRESULT hr{ S_OK };

        const wchar_t* low_freq_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\low_freq_perlin_worley.dds";
        _ASSERT_EXPR(std::filesystem::exists(low_freq_noise_tex_path), "ファイルが存在しません");
        {
            low_freq_perlin_worley_srv_ = ResourceManager::Instance().LoadTextureFromFile(dev_.Get(), low_freq_noise_tex_path);
        }
        const wchar_t* mid_freq_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\mid_freq_worley.dds";
        _ASSERT_EXPR(std::filesystem::exists(mid_freq_noise_tex_path), "ファイルが存在しません");
        {
            mid_freq_worley_srv_ = ResourceManager::Instance().LoadTextureFromFile(dev_.Get(), mid_freq_noise_tex_path);
        }
        const wchar_t* high_freq_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\high_freq_worley.dds";
        _ASSERT_EXPR(std::filesystem::exists(high_freq_noise_tex_path), "ファイルが存在しません");
        {
            high_freq_worley_srv_ = ResourceManager::Instance().LoadTextureFromFile(dev_.Get(), high_freq_noise_tex_path);
        }

        const wchar_t* weather_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\weather.bmp";
        weather_map_srv_ = ResourceManager::Instance().LoadTextureFromFile(dev_.Get(), weather_noise_tex_path);
        const wchar_t* curl_noise_tex_path = L".\\resources\\sprite\\volumetric_cloud_noises\\curl_noise.png";
        curl_noise_srv_ = ResourceManager::Instance().LoadTextureFromFile(dev_.Get(), curl_noise_tex_path);
    }

    // 進行状態初期化
    dirty_ = true;
    prefilter_next_face_ = 0;
    prefilter_next_mip_ = 0;
}

// 背景更新直後に呼ぶ：背景スクリーン（back_fb）の SRV を受け取る
void IBLManager::UpdateEnvironmentCapture(const FrameBuffer& back_fb)
{
    // ※ FrameBuffer::GetShaderResourceView に const オーバーロードが必要
    env_source_srv_ = back_fb.GetShaderResourceView(0);

    //背景リソースが変更したので保存フラグを立てる
    //want_save_dds_ = true;
}

// 背景ソース（LatLong/Equirect 2D）→ SkyCube（未フィルタ）へ変換（mip0）
void IBLManager::BuildSkyCubeFromEnvSource()
{

    if(!env_source_srv_ ||!sky_cube_ps_ || !sky_cube_uav_)
        return;

    //viewport設定
    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(kSkyCubeSize);
    vp.Height = static_cast<float>(kSkyCubeSize);
    vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
    ctx_->RSSetViewports(1, &vp);

    //RTV
    ID3D11RenderTargetView* sky_rtv = sky_cube_rtvs_[sky_cube_next_face_].Get();

    ctx_->OMSetRenderTargets(1, &sky_rtv, nullptr);

    const float clear[4] = { 0,0,0,0 };
    ctx_->ClearRenderTargetView(sky_rtv, clear);

    //定数更新
    SkyCubeCB cb{};
    cb.faceIndex = sky_cube_next_face_;
    ctx_->UpdateSubresource(cb_sky_cube_.Get(), 0, nullptr, &cb, 0, 0);

    //入力SRV
    ID3D11SamplerState* sampls[] = { samp_linear_clamp_.Get() };

    ctx_->PSSetConstantBuffers(0, 1, cb_sky_cube_.GetAddressOf());

    //フルスクリーンクワッド
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx_->VSSetShader(ibl_screen_vs_.Get(), nullptr, 0);

    ctx_->PSSetShader(sky_cube_ps_.Get(), nullptr, 0);
    

    ctx_->Draw(3, 0);

    //アンバインド
    //ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    //ctx_->PSSetShaderResources(0, 1, nullSRV);

    ctx_->GSSetShader(nullptr, nullptr, 0);
    ctx_->PSSetShader(nullptr, nullptr, 0);
    ctx_->VSSetShader(nullptr, nullptr, 0);
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);

    if (cloud_flag_||!cloud_cube_ps_||!cloud_cube_uav_)
    {

        //viewport設定
        D3D11_VIEWPORT vp{};
        vp.Width = static_cast<float>(kSkyCubeSize);
        vp.Height = static_cast<float>(kSkyCubeSize);
        vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
        ctx_->RSSetViewports(1, &vp);

        //RTV
        ID3D11RenderTargetView* rtv = cloud_cube_rtvs_[sky_cube_next_face_].Get();

        ctx_->OMSetRenderTargets(1, &rtv, nullptr);

        const float clear[4] = { 0,0,0,0 };
        ctx_->ClearRenderTargetView(rtv, clear);

        //定数更新
        SkyCubeCB cb{};
        cb.faceIndex = sky_cube_next_face_;
        ctx_->UpdateSubresource(cb_sky_cube_.Get(), 0, nullptr, &cb, 0, 0);

        ID3D11ShaderResourceView* srvs[] = { 
            low_freq_perlin_worley_srv_.Get(),
                high_freq_worley_srv_.Get(),
                weather_map_srv_.Get(),
                curl_noise_srv_.Get(),
                sky_cube_srv_.Get()
        };
        Graphics::Instance().SetShaderResource(0, _countof(srvs), srvs);
        ctx_->PSSetShaderResources(0, _countof(srvs), srvs);

        ctx_->IASetInputLayout(nullptr);
        ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx_->VSSetShader(ibl_screen_vs_.Get(), nullptr, 0);
        ctx_->PSSetShader(cloud_cube_ps_.Get(), nullptr, 0);

        
        ctx_->Draw(3, 0);

        Graphics::Instance().ClearShaderResourceViews(0, _countof(srvs));
        ctx_->PSSetShader(nullptr, nullptr, 0);
        ctx_->VSSetShader(nullptr, nullptr, 0);

        ctx_->OMSetRenderTargets(0, nullptr, nullptr);

    }

    ctx_->PSSetShaderResources(100, 1, cloud_cube_srv_.GetAddressOf());

    //最後
    dirty_ = true;

    sky_cube_next_face_++;
    if (sky_cube_next_face_ >= kCubeFaces)
    {
        sky_cube_next_face_ = 0;
        want_save_dds_ = true;

    }
}

void IBLManager::UpdateDiffuseSH()
{
    // まずは簡易：L0 のみ（後で SH 積分に置換可）
    SH9Constants sh{};
    for (int i = 0; i < 9; ++i) sh.c[i] = DirectX::XMFLOAT3(0, 0, 0);
    sh.c[0] = DirectX::XMFLOAT3(0.03f, 0.03f, 0.03f);
    ctx_->UpdateSubresource(cb_sh_.Get(), 0, nullptr, &sh, 0, 0);
}

// 1フレームに 1 face×1 mip を焼く（軽量分割）
void IBLManager::UpdateSpecularPrefilter()
{
    if (!sky_cube_srv_ || !ps_prefilter_ || !ibl_screen_vs_ || prefilter_rtvs_.empty())
        return;

    const UINT mipCount = CalcMipCount(kPrefilterSize);
    const UINT face = prefilter_next_face_;
    const UINT mip = prefilter_next_mip_;

    const UINT w = (std::max)(1u, kPrefilterSize >> mip);
    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(w);
    vp.Height = static_cast<float>(w);
    vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
    ctx_->RSSetViewports(1, &vp);

    const UINT flatIndex = face * mipCount + mip;
    ID3D11RenderTargetView* rtv = prefilter_rtvs_[flatIndex].Get();
    const float clear[4] = { 0,0,0,0 };
    ctx_->OMSetRenderTargets(1, &rtv, nullptr);
    ctx_->ClearRenderTargetView(rtv, clear);

    const float roughness = (mipCount > 1) ? (float)mip / (float)(mipCount - 1) : 0.0f;
    PrefilterCB cb{};
    cb.roughness = roughness;
    cb.faceIndex = face; 
    cb.mip_count = (float)mipCount;
    ctx_->UpdateSubresource(cb_prefilter_.Get(), 0, nullptr, &cb, 0, 0);

    // 入力は SkyCube（TextureCube）
    ID3D11ShaderResourceView* srvs[1] = { sky_cube_srv_.Get() };
    ID3D11SamplerState* samps[1] = { samp_linear_clamp_.Get() };
    ctx_->PSSetShaderResources(0, 1, srvs);
    ctx_->PSSetSamplers(0, 1, samps);
    ctx_->PSSetConstantBuffers(0, 1, cb_prefilter_.GetAddressOf());

    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(ibl_screen_vs_.Get(), nullptr, 0);
    ctx_->PSSetShader(ps_prefilter_.Get(), nullptr, 0);

    ctx_->Draw(3, 0);

    // アンバインド
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    ctx_->PSSetShaderResources(0, 1, nullSRV);
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    ctx_->VSSetShader(nullptr, nullptr, 0);
    ctx_->PSSetShader(nullptr, nullptr, 0);

    // 進行
    prefilter_next_face_++;
    if (prefilter_next_face_ >= kCubeFaces) {
        prefilter_next_face_ = 0;
        prefilter_next_mip_++;
        if (prefilter_next_mip_ >= mipCount) {
            prefilter_next_mip_ = 0;
            dirty_ = false; // 全面×mip 焼成完了

            //DDS保存
            if (want_save_dds_)
            {
                //SaveTextureToDDS(
                //    prefilter_tex_.Get(),
                //    L".\\resources\\sprite\\cube_maps\\prefiltered_env.dds", false
                //);

                SaveTextureToDDS(
                    cloud_cube_tex_.Get(),
                    L".\\resources\\sprite\\cube_maps\\sky_cube.dds", false
                );

                want_save_dds_ = false;
            }
        }
    }
}

void IBLManager::BindForObjectPass(ID3D11DeviceContext* ctx)
{
    ID3D11ShaderResourceView* srvs[3] = {
        nullptr,
        srv_pref_env_.Get(),
        srv_brdf_lut_.Get()
    };
    ctx->PSSetShaderResources(0, 3, srvs);
    ctx->PSSetConstantBuffers(2, 1, cb_sh_.GetAddressOf());

    ID3D11SamplerState* s[1] = { samp_linear_clamp_.Get() };
    ctx->PSSetSamplers(0, 1, s);
}

void IBLManager::SaveTextureToDDS(ID3D11Texture2D* tex, const wchar_t* filepath,bool force_srgb)
{
    if (!tex || !dev_ ||!ctx_) return;

    DirectX::ScratchImage image;
    //GPU->CPU
    HRESULT hr = CaptureTexture(dev_.Get(), ctx_.Get(), tex, image);
    if (FAILED(hr))
    {
        HRTrace(hr); // ログだけ出す
        return;
    }
    //
    DirectX::TexMetadata meta = image.GetMetadata();

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);
    if (desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
    {
        //キューブマップ
        meta.miscFlags |= DirectX::TEX_MISC_TEXTURECUBE;
        meta.arraySize =desc.ArraySize;//
        meta.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
    }

    //_ASSERT(!(meta.miscFlags & DirectX::TEX_MISC_TEXTURECUBE) || meta.arraySize == 1);


    //セーブ
    hr = DirectX::SaveToDDSFile(
        image.GetImages(), image.GetImageCount(), meta, DirectX::DDS_FLAGS_NONE, filepath);
    if (FAILED(hr))
    {
        HRTrace(hr); // ログだけ出す
        return;
    }

}