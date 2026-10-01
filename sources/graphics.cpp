#include"../headers/graphics.h"

#include<iostream>

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
    this->hwnd_11_ = hwnd;
    hwnd_12_ = hwnd;

    //画面サイズの取得
    RECT rect;
    GetClientRect(this->hwnd_11_, &rect);
    UINT screen_width = rect.right - rect.left;
    UINT screen_height = rect.bottom - rect.top;

    this->screen_width_ = static_cast<float>(screen_width);
    this->screen_height_ = static_cast<float>(screen_height);

	HRESULT hr{ S_OK };

    InitializeDirectX11();
    InitializeDirectX12();

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

    //2026/09/28
	//DirectX12の終了処理
	CloseHandle(dx12_fence_event_);
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

void Graphics::BeginFrameDX12()
{
    HRESULT hr{ S_OK };

    // 前フレームのGPU処理が完了するまで待つ
    if (dx12_fence_value_ > 0 &&
        dx12_fence_->GetCompletedValue() < dx12_fence_value_)
    {
        hr = dx12_fence_->SetEventOnCompletion(
            dx12_fence_value_, dx12_fence_event_);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        WaitForSingleObject(dx12_fence_event_, INFINITE);
    }

    // コマンド記録を開始
    hr = cmd_allocater_->Reset();
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    hr = cmd_list_->Reset(cmd_allocater_.Get(), nullptr);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // 現在のバックバッファを取得
    back_buffer_index_ = dx12_swap_chain_->GetCurrentBackBufferIndex();

    // バックバッファを描画可能な状態へ遷移
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource =
        dx12_back_buffers_[back_buffer_index_].Get();
    barrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_RENDER_TARGET;

    cmd_list_->ResourceBarrier(1, &barrier);

    // 現在のバックバッファに対応するRTVを設定
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
        rtv_heap_->GetCPUDescriptorHandleForHeapStart();

    rtv_handle.ptr +=
        static_cast<SIZE_T>(back_buffer_index_) * rtv_descriptor_size_;

    cmd_list_->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

    // DX12のビューポートとシザー矩形を設定
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = screen_width_;
    viewport.Height = screen_height_;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    cmd_list_->RSSetViewports(1, &viewport);

    D3D12_RECT scissor_rect{
        0,
        0,
        static_cast<LONG>(screen_width_),
        static_cast<LONG>(screen_height_)
    };
    cmd_list_->RSSetScissorRects(1, &scissor_rect);
}

void Graphics::ViewClearDX12(float r, float g, float b, float a)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
        rtv_heap_->GetCPUDescriptorHandleForHeapStart();

    rtv_handle.ptr +=
        static_cast<SIZE_T>(back_buffer_index_) * rtv_descriptor_size_;

    const float clear_color[] = { r, g, b, a };
    cmd_list_->ClearRenderTargetView(
        rtv_handle, clear_color, 0, nullptr);
}

void Graphics::PresentDX12(UINT sync_interval)
{
    HRESULT hr{ S_OK };

    // バックバッファを表示可能な状態へ戻す
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource =
        dx12_back_buffers_[back_buffer_index_].Get();
    barrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_PRESENT;

    cmd_list_->ResourceBarrier(1, &barrier);

    // コマンド記録を終了してGPUへ送る
    hr = cmd_list_->Close();
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    ID3D12CommandList* command_lists[] = { cmd_list_.Get() };
    cmd_queue_->ExecuteCommandLists(1, command_lists);

    // 画面へ表示
    hr = dx12_swap_chain_->Present(sync_interval, 0);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // GPUの完了位置を記録する
    ++dx12_fence_value_;
    hr = cmd_queue_->Signal(dx12_fence_.Get(), dx12_fence_value_);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> Graphics::CreatePipelineState(
    ID3D12RootSignature* root_signature,
    D3D12_SHADER_BYTECODE vertex_shader,
    D3D12_SHADER_BYTECODE pixel_shader,
    PipelineKind kind)
{
    _ASSERT_EXPR(dx12_device_ != nullptr, L"DX12 device is not initialized");
    _ASSERT_EXPR(root_signature != nullptr, L"Root signature is null");
    _ASSERT_EXPR(
        vertex_shader.pShaderBytecode != nullptr &&
        vertex_shader.BytecodeLength > 0,
        L"Vertex shader bytecode is empty");
    _ASSERT_EXPR(
        pixel_shader.pShaderBytecode != nullptr &&
        pixel_shader.BytecodeLength > 0,
        L"Pixel shader bytecode is empty");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = root_signature;
    desc.VS = vertex_shader;
    desc.PS = pixel_shader;

    // ラスタライザー設定
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.RasterizerState.MultisampleEnable = FALSE;
    desc.RasterizerState.AntialiasedLineEnable = FALSE;
    desc.RasterizerState.ForcedSampleCount = 0;
    desc.RasterizerState.ConservativeRaster =
        D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // 深度・ステンシル設定
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    desc.DepthStencilState.StencilEnable = FALSE;
    desc.DepthStencilState.StencilReadMask =
        D3D12_DEFAULT_STENCIL_READ_MASK;
    desc.DepthStencilState.StencilWriteMask =
        D3D12_DEFAULT_STENCIL_WRITE_MASK;

	// ステンシル設定（表面）
	desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	// ステンシル設定（裏面）
	desc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	desc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	desc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	desc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    // ブレンド設定（初期値はブレンドなし）
    auto& blend = desc.BlendState.RenderTarget[0];
    blend.BlendEnable = FALSE;
    blend.LogicOpEnable = FALSE;
    blend.SrcBlend = D3D12_BLEND_ONE;
    blend.DestBlend = D3D12_BLEND_ZERO;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    switch (kind)
    {
    case PipelineKind::Background:
        // 背景は深度テストなし、カリングなし
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        break;

    case PipelineKind::Opaque:
        // 不透明描画は深度テスト・深度書き込みあり
        desc.DepthStencilState.DepthEnable = TRUE;
        desc.DepthStencilState.DepthWriteMask =
            D3D12_DEPTH_WRITE_MASK_ALL;
        break;

    case PipelineKind::Transparent:
        // 透明描画は深度テストあり、深度書き込みなし
        desc.DepthStencilState.DepthEnable = TRUE;
        desc.DepthStencilState.DepthWriteMask =
            D3D12_DEPTH_WRITE_MASK_ZERO;

        blend.BlendEnable = TRUE;
        blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        break;

    default:
        _ASSERT_EXPR(false, L"Unknown PipelineKind");
        return nullptr;
    }

    // レンダーターゲットとサンプル設定
    desc.SampleMask = UINT_MAX;
    desc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;

    if (desc.DepthStencilState.DepthEnable)
    {
        desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;

    HRESULT hr = dx12_device_->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(pipeline_state.GetAddressOf()));

    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    return pipeline_state;
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

void Graphics::InitializeDirectX11()
{
    HRESULT hr{ S_OK };

    //デバイス＆スワップチェーン生成
    {
        UINT create_device_flags = 0;
#if defined(_DEBUG)||defined(DEBUG)
        create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL feature_levels[] =
        {
            D3D_FEATURE_LEVEL_11_1,
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
            swap_chain_desc.OutputWindow = hwnd_11_;
            swap_chain_desc.SampleDesc.Count = 1;
            swap_chain_desc.SampleDesc.Quality = 0;
            swap_chain_desc.Windowed = true;

            swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            swap_chain_desc.Flags = 0;

        }
        D3D_FEATURE_LEVEL feature_level{};

        //デバイス＆スワップチェーンの生成
        for (auto lv : feature_levels)
        {

            hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, create_device_flags,
                feature_levels, ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &swap_chain_desc,
                swap_chain_.GetAddressOf(), device_.GetAddressOf(), NULL, immediate_context_.GetAddressOf());
            if (hr == S_OK)
            {
                feature_level = lv;
                break;
            }
        }
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
}

void Graphics::InitializeDirectX12()
{
    HRESULT hr{ S_OK };

    //デバイス＆スワップチェーン生成
    {
        UINT create_device_flags = 0;
#if defined(_DEBUG)||defined(DEBUG)
        create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL feature_levels[] =
        {
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
        };

        D3D_FEATURE_LEVEL feature_level{};



        //複数のグラフィックスボードの場合
        // 特定のブラフィックスボードをアダプターするためのコード
        hr = CreateDXGIFactory1(IID_PPV_ARGS(&this->idxgi_factory_));
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        //アダプターの列挙
        std::vector<IDXGIAdapter*>adapters;

        //ここに特定の名前を持つアダプターオブジェクトが入
        IDXGIAdapter* temp_adapter = nullptr;
        for (int i = 0; idxgi_factory_->EnumAdapters(i, &temp_adapter) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            adapters.emplace_back(temp_adapter);
        }

        //アダプターを識別するための情報をループで取得
        for (auto adpt : adapters)
        {
            DXGI_ADAPTER_DESC adesc{};
            hr = adpt->GetDesc(&adesc);//アダプターの説明オブジェクト取得
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));


            std::wstring std_desc = adesc.Description;

            //探したいアダプターの名前を確認
            if (std_desc.find(L"NVIDIA") != std::string::npos)
            {
                temp_adapter = adpt;
                break;
            }
        }

        //DX12デバイスの作成
        for (auto dx12_lv : feature_levels)
        {
            hr = D3D12CreateDevice(temp_adapter, dx12_lv, IID_PPV_ARGS(&dx12_device_));
            if (hr == S_OK)
            {
                break;
            }

        }
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    }

    //DX12
    //コマンドリストの制作とコマンドアロケーター
    const auto command_list_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    {
        hr = dx12_device_->CreateCommandAllocator(command_list_type, IID_PPV_ARGS(&cmd_allocater_));
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hr = dx12_device_->CreateCommandList(0, command_list_type,
            cmd_allocater_.Get(), nullptr, IID_PPV_ARGS(&cmd_list_));
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    //DX12
    //コマンドキューの作成
    {
        D3D12_COMMAND_QUEUE_DESC queue_desc{};

        //タイムアウトなし
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        //アダプターを一つしか使わないときは0で良い
        queue_desc.NodeMask = 0;
        //プライオリティは特に指定なし
        queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        //コマンドリストと合わせる
        queue_desc.Type = command_list_type;

        //キュー生成
        hr = dx12_device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&cmd_queue_));
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		//コマンドリストを閉じる
        hr = cmd_list_->Close();
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		//フェンスの作成
        hr = dx12_device_->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(dx12_fence_.GetAddressOf())
        );
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		dx12_fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		_ASSERT_EXPR(dx12_fence_event_ != nullptr, L"CreateEvent failed!!");

        // スワップチェーンの作成
        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
        swap_chain_desc.Width =
            static_cast<UINT>(Graphics::Instance().GetScreenWidth());
        swap_chain_desc.Height =
            static_cast<UINT>(Graphics::Instance().GetScreenHeight());
        swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc.SampleDesc.Count = 1;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.BufferCount = 2;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain1;

        hr = idxgi_factory_->CreateSwapChainForHwnd(
            cmd_queue_.Get(),       // DX12ではデバイスではなくコマンドキュー
            hwnd_12_,
            &swap_chain_desc,
            nullptr,
            nullptr,
            swap_chain1.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));//ここでエラー

        hr = swap_chain1.As(&dx12_swap_chain_);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        back_buffer_index_ = dx12_swap_chain_->GetCurrentBackBufferIndex();

        // RTVディスクリプターヒープ
        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_heap_desc.NumDescriptors = swap_chain_desc.BufferCount;
        rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        hr = dx12_device_->CreateDescriptorHeap(
            &rtv_heap_desc, IID_PPV_ARGS(rtv_heap_.GetAddressOf()));
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        rtv_descriptor_size_ =
            dx12_device_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // 各バックバッファとRTVを取得・作成
        dx12_back_buffers_.resize(swap_chain_desc.BufferCount);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
            rtv_heap_->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < swap_chain_desc.BufferCount; ++i)
        {
            hr = dx12_swap_chain_->GetBuffer(
                i, IID_PPV_ARGS(dx12_back_buffers_[i].GetAddressOf()));
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

            dx12_device_->CreateRenderTargetView(
                dx12_back_buffers_[i].Get(), nullptr, rtv_handle);

            rtv_handle.ptr += rtv_descriptor_size_;
        }
    }

}

