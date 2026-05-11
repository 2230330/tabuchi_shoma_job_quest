#include"../headers/graphics.h"

#include"../headers/render_state.h"
#include"../headers/misc.h"

Graphics::~Graphics() = default;

Graphics& Graphics::Instance()
{
    static Graphics instance_;
    return instance_;
}

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
            //D3D_FEATURE_LEVEL_10_1,
            //D3D_FEATURE_LEVEL_10_0,
            //D3D_FEATURE_LEVEL_9_3,
            //D3D_FEATURE_LEVEL_9_2,
            //D3D_FEATURE_LEVEL_9_1,
        };

        //スワップチェーンを作成するための設定オプション
        DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
        {

            swap_chain_desc.BufferCount = 1;
            swap_chain_desc.BufferDesc.Width = static_cast<UINT>(Graphics::Instance().GetScreenWidth());
            swap_chain_desc.BufferDesc.Height = static_cast<UINT>(Graphics::Instance().GetScreenHeight());
            swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
            swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
            swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swap_chain_desc.OutputWindow = hwnd;
            swap_chain_desc.SampleDesc.Count = 1;
            swap_chain_desc.SampleDesc.Quality = 0;
            swap_chain_desc.Windowed = true;

            swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            swap_chain_desc.Flags = 0;

        }
        D3D_FEATURE_LEVEL feature_level{};

        //デバイス＆スワップチェーンの生成
        hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, create_device_flags,
            feature_levels, ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &swap_chain_desc,
            swap_chain_.GetAddressOf(), device_.GetAddressOf(), NULL, immediate_context_.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));


    }

    //レンダーターゲットビューの生成
    {
        //スワップチェーンからバックバッファテクスチャを取得する
        //スワップチェーンに内包されているバックバッファテクスチャは色を書き込むテクスチャ
        Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer{};
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
        D3D11_TEXTURE2D_DESC texture2d_desc{};
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

void Graphics::Finalize()
{ 
    this->ClearShaderResourceViews(0, 128);
    immediate_context_->ClearState();
    immediate_context_->Flush();
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
    HRESULT hr{ S_OK };
    hr = swap_chain_->Present(sync_interval, 0);
    
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

RenderState* Graphics::GetRenderState()
{
    return this->render_state_.get();
}

void Graphics::SetConstantBuffer(int start_slot, int num, ID3D11Buffer* const* constant_buffers)
{
    immediate_context_->VSSetConstantBuffers(start_slot, num, constant_buffers);
    immediate_context_->HSSetConstantBuffers(start_slot, num, constant_buffers);
    immediate_context_->DSSetConstantBuffers(start_slot, num, constant_buffers);
    immediate_context_->GSSetConstantBuffers(start_slot, num, constant_buffers);
    immediate_context_->PSSetConstantBuffers(start_slot, num, constant_buffers);
    immediate_context_->CSSetConstantBuffers(start_slot, num, constant_buffers);
}

void Graphics::SetShaderResource(int start_slot, int num, ID3D11ShaderResourceView* const* shader_resources)
{
    immediate_context_->VSSetShaderResources(start_slot, num, shader_resources);
    immediate_context_->HSSetShaderResources(start_slot, num, shader_resources);
    immediate_context_->DSSetShaderResources(start_slot, num, shader_resources);
    immediate_context_->GSSetShaderResources(start_slot, num, shader_resources);
    immediate_context_->PSSetShaderResources(start_slot, num, shader_resources);
    immediate_context_->CSSetShaderResources(start_slot, num, shader_resources);
}

void Graphics::SetSampler(int start_slot, int num, ID3D11SamplerState* const* sampler_state)
{
    immediate_context_->VSSetSamplers(start_slot, num, sampler_state);
    immediate_context_->HSSetSamplers(start_slot, num, sampler_state);
    immediate_context_->DSSetSamplers(start_slot, num, sampler_state);
    immediate_context_->GSSetSamplers(start_slot, num, sampler_state);
    immediate_context_->PSSetSamplers(start_slot, num, sampler_state);
    immediate_context_->CSSetSamplers(start_slot, num, sampler_state);
}

void Graphics::ClearShaderSlots()
{
    immediate_context_->VSSetShader(nullptr, nullptr, 0);
    immediate_context_->HSSetShader(nullptr, nullptr, 0);
    immediate_context_->DSSetShader(nullptr, nullptr, 0);
    immediate_context_->GSSetShader(nullptr, nullptr, 0);
    immediate_context_->PSSetShader(nullptr, nullptr, 0);
    immediate_context_->CSSetShader(nullptr, nullptr, 0);
}
//コンスタントバッファを消すための関数
//スロットに要素を入れるときは、必ずナンバーまで記載する事
//でなければ、範囲外になってしまう場合がある
void Graphics::ClearConstantBuffers(int start_slot, int num)
{
    ID3D11Buffer* clear_constant_bufferes[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
    immediate_context_->VSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
    immediate_context_->HSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
    immediate_context_->DSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
    immediate_context_->GSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
    immediate_context_->PSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
    immediate_context_->CSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
}

void Graphics::ClearShaderResourceViews(int start_slot, int num)
{
    ID3D11ShaderResourceView* clear_shader_resource_view[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
    immediate_context_->VSSetShaderResources(start_slot, num, clear_shader_resource_view);
    immediate_context_->HSSetShaderResources(start_slot, num, clear_shader_resource_view);
    immediate_context_->DSSetShaderResources(start_slot, num, clear_shader_resource_view);
    immediate_context_->GSSetShaderResources(start_slot, num, clear_shader_resource_view);
    immediate_context_->PSSetShaderResources(start_slot, num, clear_shader_resource_view);
    immediate_context_->CSSetShaderResources(start_slot, num, clear_shader_resource_view);
}

void Graphics::ClearSampler(int start_slot, int num)
{
    ID3D11SamplerState* clear_sampler[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT]{};
    immediate_context_->VSSetSamplers(start_slot, num, clear_sampler);
    immediate_context_->HSSetSamplers(start_slot, num, clear_sampler);
    immediate_context_->DSSetSamplers(start_slot, num, clear_sampler);
    immediate_context_->GSSetSamplers(start_slot, num, clear_sampler);
    immediate_context_->PSSetSamplers(start_slot, num, clear_sampler);
    immediate_context_->CSSetSamplers(start_slot, num, clear_sampler);
}
