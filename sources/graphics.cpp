#include"../headers/graphics.h"
#include"../headers/misc.h"

void Graphics::Initialize(HWND hwnd)
{
    this->hwnd_ = hwnd;

    //画面サイズの取得
    RECT rect;
    GetClientRect(this->hwnd_, &rect);
    UINT screen_width = rect.right - rect.left;
    UINT screen_height = rect.bottom - rect.top;

    this->screen_width_ = static_cast<float>(screen_width);
    this->screen_height_ = static_cast<float>(screen_height);

    HRESULT hr{ S_OK };

    //デバイス＆スワップチェーン生成
    {
        UINT create_device_flags = 0;
#if defined(_DEBUG)||defined(DEBUG)
        create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL feature_levels[] =
        {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1,
        };

        //スワップチェーンを作成するための設定オプション
        DXGI_SWAP_CHAIN_DESC swap_chain_desc;
        {
            swap_chain_desc.BufferCount = 2;
            swap_chain_desc.BufferDesc.Width = screen_width;
            swap_chain_desc.BufferDesc.Height = screen_height;
            swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
            swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
            swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            swap_chain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
            swap_chain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
            swap_chain_desc.SampleDesc.Count = 1;
            swap_chain_desc.SampleDesc.Quality = 0;
            swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swap_chain_desc.OutputWindow = this->hwnd_;
            swap_chain_desc.Windowed = TRUE;
            swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }
        D3D_FEATURE_LEVEL feature_level;

        //デバイス＆スワップチェーンの生成
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            create_device_flags,
            feature_levels,
            ARRAYSIZE(feature_levels),
            D3D11_SDK_VERSION,
            &swap_chain_desc,
            swap_chain_.GetAddressOf(),
            device_.GetAddressOf(),
            &feature_level,
            immediate_context_.GetAddressOf()
        );
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    //レンダーターゲットビューの生成
    {
        //スワップチェーンからバックバッファテクスチャを取得する
        //スワップチェーンに内包されているバックバッファテクスチャは色を書き込むテクスチャ
        Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
        hr = swap_chain_->GetBuffer(
            0,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(back_buffer.GetAddressOf())
        );
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        //バックバッファテクスチャへの書き込みの窓口となるレンダーターゲットビューを生成する
        hr = device_->CreateRenderTargetView(
            back_buffer.Get(),
            nullptr,
            render_target_view_.GetAddressOf()
        );
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    //深度ステンシルビューの生成
    {
        //深度ステンシル情報を書き込むためのテクスチャを作成
        ID3D11Texture2D* depth_stencil_buffer{};
        D3D11_TEXTURE2D_DESC texture2d_desc;
        texture2d_desc.Width = screen_width;
        texture2d_desc.Height = screen_height;
        texture2d_desc.MipLevels = 1;
        texture2d_desc.ArraySize = 1;
        texture2d_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        texture2d_desc.SampleDesc.Count = 1;
        texture2d_desc.SampleDesc.Quality = 0;
        texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
        texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        texture2d_desc.CPUAccessFlags = 0;
        texture2d_desc.MiscFlags = 0;
        hr = device_.Get()->CreateTexture2D(&texture2d_desc, NULL, &depth_stencil_buffer);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        //深度ステンシルテクスチャへの書き込みに窓口になる深度ステンシルビューを作成する
        D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
        depth_stencil_view_desc.Format = texture2d_desc.Format;
        depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        depth_stencil_view_desc.Texture2D.MipSlice = 0;
        hr = device_.Get()->CreateDepthStencilView(depth_stencil_buffer, &depth_stencil_view_desc, depth_stencil_view_.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        depth_stencil_buffer->Release();
    }

    //ビューポート
    {
        viewport_.Width = static_cast<float>(screen_width);
        viewport_.Height = static_cast<float>(screen_height);
        viewport_.MinDepth = 0.f;
        viewport_.MaxDepth = 1.f;
        viewport_.TopLeftX = 0.f;
        viewport_.TopLeftY = 0.f;
    }

    //レンダーステート生成
    render_state_ = std::make_unique<RenderState>(this->device_.Get());
}

//画面のクリア
void Graphics::ViewClear(float r, float g, float b, float a)
{
    float color[4]{ r,g,b,a };
    immediate_context_->ClearRenderTargetView(render_target_view_.Get(), color);
    immediate_context_->ClearDepthStencilView(depth_stencil_view_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

//レンダーターゲット設定
void Graphics::SetRenderTargets()
{
    immediate_context_->RSSetViewports(1, &viewport_);
    immediate_context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), depth_stencil_view_.Get());
}

//画面表示
void Graphics::Present(UINT sync_interval)
{
    swap_chain_->Present(sync_interval, DXGI_PRESENT_ALLOW_TEARING);
}