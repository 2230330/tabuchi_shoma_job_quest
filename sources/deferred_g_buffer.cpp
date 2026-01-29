#include"../headers/deferred_g_buffer.h"

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
        DXGI_FORMAT_R16G16B16A16_FLOAT, // BaseColor
        DXGI_FORMAT_R16G16B16A16_FLOAT, // Emissive
        DXGI_FORMAT_R16G16B16A16_FLOAT  // Normal + Depth
    };

    UINT quality = 0;
    if (enable_msaa)
    {
        hr = device->CheckMultisampleQualityLevels(formats[0], subsamples, &quality);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    for (uint8_t i = 0; i < kRTCount; ++i)
    {
        D3D11_TEXTURE2D_DESC tex{};
        tex.Width = width;
        tex.Height = height;
        tex.MipLevels = 1;
        tex.ArraySize = 1;
        tex.Format = formats[i];
        tex.SampleDesc.Count = enable_msaa ? subsamples : 1;
        tex.SampleDesc.Quality = enable_msaa ? quality - 1 : 0;
        tex.Usage = D3D11_USAGE_DEFAULT;
        tex.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        hr = device->CreateTexture2D(&tex, nullptr, render_targets_[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hr = device->CreateRenderTargetView(render_targets_[i].Get(), nullptr, rtvs_[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hr = device->CreateShaderResourceView(render_targets_[i].Get(), nullptr, shader_resource_views_[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

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
}
void DeferredGBuffer::Clear(ID3D11DeviceContext* ctx)
{
    const FLOAT clear[4] = { 0,0,0,0 };

    for (uint8_t i = 0; i < kRTCount; ++i)
    {
        ctx->ClearRenderTargetView(rtvs_[i].Get(), clear);
    }
    ctx->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void DeferredGBuffer::Activate(ID3D11DeviceContext* ctx)
{
    viewport_count_ = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ctx->RSGetViewports(&viewport_count_, cached_viewports_);

    ctx->OMGetRenderTargets(kRTCount,
        reinterpret_cast<ID3D11RenderTargetView**>(cached_rtvs_),
        cached_dsv_.ReleaseAndGetAddressOf());

    ctx->RSSetViewports(1, &viewport_);

    ID3D11RenderTargetView* rtvs[kRTCount];
    for (uint8_t i = 0; i < kRTCount; ++i)
        rtvs[i] = rtvs_[i].Get();

    ctx->OMSetRenderTargets(kRTCount, rtvs, dsv_.Get());
}

void DeferredGBuffer::Deactivate(ID3D11DeviceContext* ctx)
{
    ctx->RSSetViewports(viewport_count_, cached_viewports_);

    ID3D11RenderTargetView* rtvs[kRTCount];
    for (uint8_t i = 0; i < kRTCount; ++i)
        rtvs[i] = cached_rtvs_[i].Get();

    ctx->OMSetRenderTargets(kRTCount, rtvs, cached_dsv_.Get());
}

