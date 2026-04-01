#include"../headers/deferred_g_buffer.h"
#include"../headers/graphics.h"
#include"../headers/misc.h"

DeferredGBuffer::DeferredGBuffer(
    ID3D11Device* device,
    uint32_t width,
    uint32_t height,
    bool enable_msaa,
    int subsamples
)
{
    HRESULT hr{ S_OK };

    DXGI_FORMAT formats[kRTCount] =
    {
        DXGI_FORMAT_R32G32B32A32_FLOAT, // BaseColor
        DXGI_FORMAT_R16G16B16A16_FLOAT, // Emissive
        DXGI_FORMAT_R32G32B32A32_FLOAT,  // Normal 
        DXGI_FORMAT_R16G16B16A16_FLOAT, // Parameter 
        DXGI_FORMAT_R32_FLOAT, // Depth 
        DXGI_FORMAT_R32G32B32A32_FLOAT, // Velocity 

    };

    UINT quality = 0;
    if (enable_msaa)
    {
        hr = device->CheckMultisampleQualityLevels(formats[0], subsamples, &quality);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    //各レンダーターゲットビューの設定
    D3D11_TEXTURE2D_DESC tex{};
    tex.Width = width;
    tex.Height = height;
    tex.MipLevels = 1;
    tex.ArraySize = 1;
    tex.SampleDesc.Count = enable_msaa ? subsamples : 1;
    tex.SampleDesc.Quality = enable_msaa ? quality - 1 : 0;
    tex.Usage = D3D11_USAGE_DEFAULT;
    tex.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    for (uint8_t i = 0; i < kRTCount; ++i)
    {
        tex.Format = formats[i];

        hr = device->CreateTexture2D(&tex, nullptr, render_targets_[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hr = device->CreateRenderTargetView(render_targets_[i].Get(), nullptr, rtvs_[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hr = device->CreateShaderResourceView(render_targets_[i].Get(), nullptr, shader_resource_views_[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    //GBuffer用ブレンドステート作成
    //GBufferの際はαブレンドを機能させるとおかしなことになる為ブレンド無し設定
    D3D11_BLEND_DESC blend_desc{};
    blend_desc.AlphaToCoverageEnable = FALSE;
    blend_desc.IndependentBlendEnable = FALSE;
    for (int i = 0; i < kRTCount; ++i)
    {
        blend_desc.RenderTarget[i].BlendEnable = FALSE;
        blend_desc.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
        blend_desc.RenderTarget[i].DestBlend = D3D11_BLEND_ZERO;
        blend_desc.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend_desc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
        blend_desc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    hr = Graphics::Instance().GetDevice()->CreateBlendState(&blend_desc, blend_state_.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // depth
    D3D11_TEXTURE2D_DESC depth{};
    depth.Width = width;
    depth.Height = height;
    depth.MipLevels = 1;
    depth.ArraySize = 1;
    depth.Format = DXGI_FORMAT_R32_TYPELESS;
    depth.SampleDesc.Count = enable_msaa ? subsamples : 1;
    depth.SampleDesc.Quality = enable_msaa ? quality - 1 : 0;
    depth.Usage = D3D11_USAGE_DEFAULT;
    depth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    hr = device->CreateTexture2D(&depth, nullptr, depth_texture_.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    D3D11_DEPTH_STENCIL_VIEW_DESC dsv{};
    dsv.Format = DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension = enable_msaa ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;

    hr = device->CreateDepthStencilView(depth_texture_.Get(), &dsv, dsv_.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R32_FLOAT;
    srv.ViewDimension = enable_msaa ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(depth_texture_.Get(), &srv, depth_srv_.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    this->render_state_ = std::make_unique<RenderState>(device);
}
void DeferredGBuffer::Clear(ID3D11DeviceContext* ctx)
{
    const FLOAT clear[kRTCount][4] =
    {
        {0,0,0,0},//BaseColor
        {0,0,0,0},//Emissive
        {0,0,0,0},//Normal
        {0,0,0,0},//Parameter
        {1,1,1,1},//Depth
        {0,0,0,0},//Velocity
    };

    for (uint8_t i = 0; i < kRTCount; ++i)
    {
        ctx->ClearRenderTargetView(rtvs_[i].Get(), clear[i]);
    }
    ctx->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void DeferredGBuffer::Activate(ID3D11DeviceContext* ctx)
{
    // viewport cache
    viewport_count_ = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ctx->RSGetViewports(&viewport_count_, cached_viewports_);

    // ---- OM cache ----
    ID3D11RenderTargetView* prevRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* prevDSV = nullptr;

    ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, prevRTVs, &prevDSV);

    // cached_rtvs_ は kRTCount しか無いので、
    // まず全部クリアしてから、必要な分だけ入れる
    for (uint8_t i = 0; i < kRTCount; ++i)
        cached_rtvs_[i].Reset();
    cached_dsv_.Reset();

    cached_num_rtvs_ = 0;

    // 先頭から連続しているRTV数を数える
    for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
        if (prevRTVs[i])
        {
            if (i < kRTCount)
            {
                cached_rtvs_[i].Attach(prevRTVs[i]); // Release責任をComPtrへ
            }
            else
            {
                // もし Activate 前が カウント以上のMRTだったら、ここで保持しきれない
                // とりあえずリークしないようにRelease
                prevRTVs[i]->Release();
            }

            cached_num_rtvs_ = i + 1;
        }
        else
        {
            // 連続前提
            break;
        }
    }

    cached_dsv_.Attach(prevDSV);

    // ---- set viewport ----
    ctx->RSSetViewports(1, &viewport_);

    // ---- bind deferred targets ----
    ID3D11RenderTargetView* rtvs[kRTCount];
    for (uint8_t i = 0; i < kRTCount; ++i)
        rtvs[i] = rtvs_[i].Get();

    ctx->OMSetRenderTargets(kRTCount, rtvs, dsv_.Get());

    ctx->OMSetBlendState(blend_state_.Get(), nullptr, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(render_state_->GetDepthStencilState(DepthState::test_and_write),0);
}

void DeferredGBuffer::Deactivate(ID3D11DeviceContext* ctx)
{
    ctx->RSSetViewports(viewport_count_, cached_viewports_);

    // 元の枚数で復元する
    ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};

    UINT restore_count = cached_num_rtvs_;
    if (restore_count > kRTCount) restore_count = kRTCount; // 念のため

    for (UINT i = 0; i < restore_count; ++i)
        rtvs[i] = cached_rtvs_[i].Get();

    ctx->OMSetRenderTargets(restore_count, rtvs, cached_dsv_.Get());

    // 二重保持防止
    for (uint8_t i = 0; i < kRTCount; ++i)
        cached_rtvs_[i].Reset();
    cached_dsv_.Reset();
    cached_num_rtvs_ = 0;
}

void DeferredGBuffer::SetDeferredCommonResource()
{
    ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
    RenderState* render_state = Graphics::Instance().GetRenderState();
    //サンプラーステート設定
    {
        ID3D11SamplerState* sampler_state[] =
        {
            render_state->GetSamplerState(SamplerState::point_wrap),
            render_state->GetSamplerState(SamplerState::point_clamp),
            render_state->GetSamplerState(SamplerState::linear_wrap),
            render_state->GetSamplerState(SamplerState::linear_clamp),
            render_state->GetSamplerState(SamplerState::anisotropic),
            render_state->GetSamplerState(SamplerState::linear_mirror),
        };
        Graphics::Instance().SetSampler(0, ARRAYSIZE(sampler_state), sampler_state);
    }

    ID3D11ShaderResourceView* srvs[Target::Count] =
    {
        shader_resource_views_[Target::BaseColor].Get(),
        shader_resource_views_[Target::Emissive].Get(),
        shader_resource_views_[Target::Normal].Get(),
        shader_resource_views_[Target::Parameter].Get(),
        shader_resource_views_[Target::Depth].Get(),
        shader_resource_views_[Target::Velocity].Get(),
    };
    Graphics::Instance().SetShaderResource(0, Target::Count, srvs);
}
