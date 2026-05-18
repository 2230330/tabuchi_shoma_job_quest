#include"../../headers/system/render_screen_space_reflection_system.h"

#include<algorithm>

#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
#include"../../headers/constant_buffer_slot.h"
#include"../../headers/misc.h"
#include"../../headers/component/component_manager.h"
#include"../../headers/framebuffer.h"
#include"../../headers/fullscreen_quad.h"


RenderScreenSpaceReflectionSystem::RenderScreenSpaceReflectionSystem(ComponentManager& comp_mng, RenderPass render_pass)
    :comp_mng_(comp_mng)
    , IRenderSystem(render_pass)
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
    //コンピュートシェーダー用の設定
    {
        //コンピュートシェーダーでHi-Z用の深度情報を作成します
        //Hi-zテクスチャ
        D3D11_TEXTURE2D_DESC texture_desc{};
        texture_desc.Width = Graphics::Instance().GetScreenWidth();
        texture_desc.Height = Graphics::Instance().GetScreenHeight();
        texture_desc.MipLevels = MIP_COUNT;
        texture_desc.ArraySize = 1;
        texture_desc.Format = DXGI_FORMAT_R32_FLOAT;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Usage = D3D11_USAGE_DEFAULT;
        texture_desc.BindFlags =
            D3D11_BIND_SHADER_RESOURCE |
            D3D11_BIND_UNORDERED_ACCESS;

        hr = device->CreateTexture2D(&texture_desc, nullptr, hiz_depth_texture_.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        //srv生成
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format = texture_desc.Format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = MIP_COUNT;
        hr = device->CreateShaderResourceView(hiz_depth_texture_.Get(), &srv_desc, hiz_depth_srv_.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hiz_uavs_.resize(MIP_COUNT);
        hiz_srvs_.resize(MIP_COUNT);

        // UAV
        for (int i = 0; i < MIP_COUNT; ++i)
        {

            D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
            uav_desc.Format = texture_desc.Format;
            uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            uav_desc.Texture2D.MipSlice = i;
            hr = device->CreateUnorderedAccessView(
                hiz_depth_texture_.Get(),
                &uav_desc,
                hiz_uavs_[i].GetAddressOf()
            );
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));


            // SRV（mip単独）
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc_mip{};
            srv_desc_mip.Format = texture_desc.Format;
            srv_desc_mip.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc_mip.Texture2D.MostDetailedMip = i;
            srv_desc_mip.Texture2D.MipLevels = 1;

            hr = device->CreateShaderResourceView(
                hiz_depth_texture_.Get(),
                &srv_desc_mip,
                hiz_srvs_[i].GetAddressOf()
            );
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));


        }
        //ComputeShader
        hiz_cs_ = ResourceManager::Instance().LoadComputeShader(
            device,
            L".\\resources\\shader\\ssr_hiz_cs.cso");



        for (int i = 0; i < 2; i++)
        {
            Microsoft::WRL::ComPtr<ID3D11Texture2D>texture2d;
            hr = device->CreateTexture2D(&texture_desc, nullptr, texture2d.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
            hr = device->CreateShaderResourceView(texture2d.Get(), nullptr, hiz_process_srvs_[i].GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
            hr = device->CreateUnorderedAccessView(texture2d.Get(), nullptr, hiz_process_uavs_[i].GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }

    }

    ssr_ps_ =
        ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\screen_space_reflection_ps.cso");
    ssr_synthesis_ps_=
        ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\screen_space_reflection_synthesis_ps.cso");


    ssr_framebuffer_ =
        std::make_unique<FrameBuffer>(device,
            static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()),
            static_cast<uint32_t>(Graphics::Instance().GetScreenHeight())
        );
    ssr_synthesis_framebuffer_ =
        std::make_unique<FrameBuffer>(device,
            static_cast<uint32_t>(Graphics::Instance().GetScreenWidth()),
            static_cast<uint32_t>(Graphics::Instance().GetScreenHeight())
        );
    ssr_fullscreen_quad_ = std::make_unique<FullscreenQuad>(device);
}

void RenderScreenSpaceReflectionSystem::Render()
{
    has_ssr_ = false;

    comp_mng_.ForEach<ComponentSsr>([&](uint32_t entity_id, ComponentSsr& ssr)
        {
            ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

            ssr_framebuffer_->Clear(context);
            ssr_framebuffer_->Activate(context, FrameBuffer::usage::color);

            ComputeHiz(context);


            //定数バッファにスクリーンスペースリフレクションのパラメータを転送
            SsrConstants ssr_constants{};
            ssr_constants.distance = ssr.distance;
            ssr_constants.num_steps = ssr.num_steps;
            ssr_constants.max_mip = ssr.max_mip;
            ssr_constants.thickness = ssr.thickness;
            ssr_constants.resolution = ssr.resolution;
            ssr_constants.intensity = ssr.intensity;

            context->UpdateSubresource(ssr_constant_buffer_.Get(), 0, nullptr, &ssr_constants, 0, 0);
            Graphics::Instance().SetConstantBuffer(ConstantBufferSlot::kSsr, 1, ssr_constant_buffer_.GetAddressOf());

            ID3D11ShaderResourceView* srvs[] = {
                normal_srv_.Get(),
                hiz_depth_srv_.Get(),
                color_srv_.Get(),
            };
            ssr_fullscreen_quad_->blit(context, srvs, 0, _countof(srvs), ssr_ps_.Get());

            Graphics::Instance().ClearShaderResourceViews(0, _countof(srvs));

            ssr_framebuffer_->Deactivate(context);

            //オブジェ画像とSSR画像を合成
            ssr_synthesis_framebuffer_->Clear(context);
            ssr_synthesis_framebuffer_->Activate(context);

            ID3D11ShaderResourceView* synthesis_srvs[] = {
                ssr_framebuffer_->GetShaderResourceView(0).Get(),
                color_srv_.Get(),
            };
            ssr_fullscreen_quad_->blit(context, synthesis_srvs, 0, _countof(synthesis_srvs), ssr_synthesis_ps_.Get());
            Graphics::Instance().ClearShaderResourceViews(0, _countof(synthesis_srvs));

            ssr_synthesis_framebuffer_->Deactivate(context);

            //確認用にコンポーネントに収納
            ssr.ssr_texture = ssr_framebuffer_->GetShaderResourceView(0).Get();
            ssr.normal = normal_srv_.Get();
            ssr.depth = depth_srv_.Get();
            ssr.color = color_srv_.Get();

            has_ssr_ = true;

        });
}

ID3D11ShaderResourceView* RenderScreenSpaceReflectionSystem::GetSSRTexture()
{
    ID3D11ShaderResourceView* srv;
    if (has_ssr_)
    {
        srv = ssr_synthesis_framebuffer_->GetShaderResourceView(0).Get();
    }
    else
    {
        srv = color_srv_.Get();
    }

    return srv;
}

void RenderScreenSpaceReflectionSystem::ComputeHiz(ID3D11DeviceContext* ctx)
{
    ctx->CSSetShader(hiz_cs_.Get(), nullptr, 0);

    int width = Graphics::Instance().GetScreenWidth();
    int height = Graphics::Instance().GetScreenHeight();

    // mip0にdepthコピー（超重要）
    ctx->CopySubresourceRegion(
        hiz_depth_texture_.Get(),
        0,
        0, 0, 0,
        hiz_copy_tex_.Get(),
        0,
        nullptr
    );

    // Mip生成
    for (uint32_t mip = 0; mip < MIP_COUNT - 1; mip++)
    {
        // 入力
        ID3D11ShaderResourceView* srv = hiz_srvs_[mip].Get();
        ctx->CSSetShaderResources(0, 1, &srv);

        // 出力
        ID3D11UnorderedAccessView* uav = hiz_uavs_[mip + 1].Get();
        ctx->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

        // サイズ
        UINT w = (std::max)(1, width >> (mip + 1));
        UINT h = (std::max)(1, height >> (mip + 1));
        w = (w + 7) / 8;
        h = (h + 7) / 8;

        ctx->Dispatch(w, h, 1);

        // バインド解除
        ID3D11UnorderedAccessView* nullUAV[1] = { nullptr };
        ctx->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

        ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
        ctx->CSSetShaderResources(0, 1, nullSRV);
    }
}
