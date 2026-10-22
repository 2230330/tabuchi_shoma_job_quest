#include"../headers/resource_manager.h"
#include<d3dcompiler.h>
#include<DDSTextureLoader.h>
#include<WICTextureLoader.h>
#include<functional>
#include<filesystem>
#include<iostream>
#include<DirectXTex.h>

#include"../headers/gltf_model.h"
#include"../headers/misc.h"

std::shared_ptr<GltfModel> ResourceManager::LoadGltfModel(ID3D11Device* device, const std::string& filename)
{
    auto it = gltf_models_.find(filename);
    if (it != gltf_models_.end())
    {
        return it->second;
    }

    std::shared_ptr<GltfModel> model = std::make_shared<GltfModel>(device, filename);
    gltf_models_.emplace(filename, model);

    return model;
}

Microsoft::WRL::ComPtr<ID3D11VertexShader> ResourceManager::LoadVertexShader(ID3D11Device* device, const std::wstring& filename,
    ID3D11InputLayout** input_layout, D3D11_INPUT_ELEMENT_DESC* input_element_desc,UINT num_element)
{

    Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
    Microsoft::WRL::ComPtr<ID3DBlob> blob;

    HRESULT hr = D3DReadFileToBlob(filename.c_str(), blob.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    auto it = vertex_shaders_.find(filename);
    if (it != vertex_shaders_.end())
    {
        if (input_layout)
        {
            hr = device->CreateInputLayout(input_element_desc, num_element,
                blob->GetBufferPointer(), blob->GetBufferSize(), input_layout);
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }
        return it->second;
    }


    if (SUCCEEDED(hr))
    {
        hr = device->CreateVertexShader(blob->GetBufferPointer(),
            blob->GetBufferSize(), nullptr, shader.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    if (FAILED(hr))
    {
        std::wstring message = L"Failed to load vertex shader: " + filename + L" Error: " + HRTrace(hr);
        OutputDebugStringW(message.c_str());
        return nullptr;
    }


    if (input_layout)
    {
        hr = device->CreateInputLayout(input_element_desc, num_element,
            blob->GetBufferPointer(), blob->GetBufferSize(), input_layout);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }



    vertex_shaders_.emplace(filename, shader);
    return shader;
}
Microsoft::WRL::ComPtr<ID3D11PixelShader> ResourceManager::LoadPixelShader(ID3D11Device* device, const std::wstring& filename)
{
    auto it = pixel_shaders_.find(filename);
    if (it != pixel_shaders_.end())
    {
        return it->second;
    }

    Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
    Microsoft::WRL::ComPtr<ID3DBlob> blob;

    HRESULT hr = D3DReadFileToBlob(filename.c_str(), blob.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    }

    if (FAILED(hr))
    {
        std::wstring message = L"Failed to load pixel shader: " + filename + L" Error: " + HRTrace(hr);
        OutputDebugStringW(message.c_str());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        return nullptr;
    }

    pixel_shaders_.emplace(filename, shader);
    return shader;
}

Microsoft::WRL::ComPtr<ID3D11ComputeShader> ResourceManager::LoadComputeShader(ID3D11Device* device, const std::wstring& filename)
{
    auto it = compute_shaders_.find(filename);
    if (it != compute_shaders_.end())
    {
        return it->second;
    }
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader;
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    HRESULT hr = D3DReadFileToBlob(filename.c_str(), blob.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    if (SUCCEEDED(hr))
    {
        hr = device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }
    if (FAILED(hr))
    {
        std::wstring message = L"Failed to load compute shader: " + filename + L" Error: " + HRTrace(hr);
        OutputDebugStringW(message.c_str());
        return nullptr;
    }
    compute_shaders_.emplace(filename, shader);
    return shader;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
ResourceManager::LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename)
{
    HRESULT hr = S_OK;
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

    // --- キャッシュ確認 ---
    auto it = textures_.find(filename);
    if (it != textures_.end())
    {
        return it->second;
    }

    std::filesystem::path dds_filename(filename);
    dds_filename.replace_extension("dds");

    //========================================================
    //  DDS かつ Texture3D かどうかを事前に見極める
    //========================================================
    if (std::filesystem::exists(dds_filename))
    {
        DirectX::ScratchImage image;
        DirectX::TexMetadata meta;

        hr = DirectX::LoadFromDDSFile(dds_filename.c_str(), DirectX::DDS_FLAGS_NONE, &meta, image);
        if (SUCCEEDED(hr))
        {
            if (meta.dimension == DirectX::TEX_DIMENSION_TEXTURE3D)
            {
                //--------------------------------------------------------
                // 3D テクスチャとして読み込む
                //--------------------------------------------------------
                D3D11_TEXTURE3D_DESC desc = {};
                desc.Width = (UINT)meta.width;
                desc.Height = (UINT)meta.height;
                desc.Depth = (UINT)meta.depth;
                desc.MipLevels = (UINT)meta.mipLevels;
                desc.Format = meta.format;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                desc.CPUAccessFlags = 0;
                desc.MiscFlags = 0;

                std::vector<D3D11_SUBRESOURCE_DATA> initData(meta.mipLevels);
                const DirectX::Image* imgs = image.GetImages();

                for (size_t mip = 0; mip < meta.mipLevels; ++mip)
                {
                    initData[mip].pSysMem = imgs[mip].pixels;
                    initData[mip].SysMemPitch = (UINT)imgs[mip].rowPitch;
                    initData[mip].SysMemSlicePitch = (UINT)imgs[mip].slicePitch;
                }

                Microsoft::WRL::ComPtr<ID3D11Texture3D> tex3D;
                hr = device->CreateTexture3D(&desc, initData.data(), tex3D.GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

                // --- SRV 作成 ---
                D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
                srvd.Format = desc.Format;
                srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                srvd.Texture3D.MipLevels = desc.MipLevels;
                srvd.Texture3D.MostDetailedMip = 0;

                hr = device->CreateShaderResourceView(tex3D.Get(), &srvd, srv.GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

                // キャッシュ登録
                textures_.insert({ filename, srv });
                return srv;
            }
        }

        // ここまで来たら → DDS だが 2D or CubeMap
        hr = DirectX::CreateDDSTextureFromFile(
            device, dds_filename.c_str(), resource.GetAddressOf(), srv.GetAddressOf()
        );
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        textures_.insert({ filename, srv });
        return srv;
    }

    //========================================================
    //  DDS 以外（PNG, JPEG など）は従来通り WIC で読み込む
    //========================================================
    hr = DirectX::CreateWICTextureFromFile(
        device, filename, resource.GetAddressOf(), srv.GetAddressOf()
    );
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    textures_.insert({ filename, srv });
    return srv;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ResourceManager::MakeDummyTexture(ID3D11Device* device, DWORD value, UINT dimension)
{
        std::wstringstream name;
        name << std::setw(8) << std::setfill(L'0') << std::hex << std::uppercase << value << L"." << dimension;

        auto it = textures_.find(name.str());
        if (it != textures_.end())
        {
            return it->second;
        }
    
    
        D3D11_TEXTURE2D_DESC texture2d_desc{};
        texture2d_desc.Width = dimension;
        texture2d_desc.Height = dimension;
        texture2d_desc.MipLevels = 1;
        texture2d_desc.ArraySize = 1;
        texture2d_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture2d_desc.SampleDesc.Count = 1;
        texture2d_desc.SampleDesc.Quality = 0;
        texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
        texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
        size_t texels = static_cast<size_t>(dimension)*dimension;
        std::vector<DWORD>sysmem(texels);
        std::fill(sysmem.begin(), sysmem.end(), value);
    
        D3D11_SUBRESOURCE_DATA subresource_data{};
        subresource_data.pSysMem = sysmem.data();
        subresource_data.SysMemPitch = sizeof(DWORD) * dimension;
    
        HRESULT hr{ S_OK };
    
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
        hr = device->CreateTexture2D(&texture2d_desc, &subresource_data, &texture2d);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    
        D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
        shader_resource_view_desc.Format = texture2d_desc.Format;
        shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shader_resource_view_desc.Texture2D.MipLevels = 1;
    
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>shader_resource_view;
        hr = device->CreateShaderResourceView(texture2d.Get(), &shader_resource_view_desc,
            shader_resource_view.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    
        textures_.emplace(std::make_pair(name.str().c_str(), shader_resource_view));
    
        return shader_resource_view;
}

//バイナリデータから八種を計算、文字にする
std::wstring ResourceManager::MakeHashKey(const void* data, size_t size)
{
    // 64bitハッシュを文字列化
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    size_t hash_value = 0;

    // Fowler–Noll–Vo hash (FNV-1a) の簡易実装
    const size_t fnv_offset = 1469598103934665603ULL;
    const size_t fnv_prime = 1099511628211ULL;

    hash_value = fnv_offset;
    for (size_t i = 0; i < size; ++i)
    {
        hash_value ^= bytes[i];
        hash_value *= fnv_prime;
    }

    // ハッシュを文字列化
    return L"memtex_" + std::to_wstring(hash_value);
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ResourceManager::LoadTextureFromMemory(ID3D11Device* device, const void* data, size_t size)
{
    //キーの作成
    std::wstring key = MakeHashKey(data, size);
    auto it = textures_.find(key);
    if (it != textures_.end())
    {
        return it->second;
    }

    HRESULT hr{ S_OK };
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>shader_resource_view;

    hr = DirectX::CreateDDSTextureFromMemory(device, reinterpret_cast<const uint8_t*>(data), size, resource.GetAddressOf(), shader_resource_view.GetAddressOf());
    if (hr != S_OK)
    {
        hr = DirectX::CreateWICTextureFromMemory(device, reinterpret_cast<const uint8_t*>(data), size, resource.GetAddressOf(), shader_resource_view.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    textures_.insert({ key,shader_resource_view });

    return shader_resource_view;
}

D3D11_TEXTURE2D_DESC ResourceManager::Texture2dDesc(ID3D11ShaderResourceView* shader_resource_view)
{
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    shader_resource_view->GetResource(resource.GetAddressOf());

    D3D11_TEXTURE2D_DESC texture2d_desc;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
    HRESULT hr = resource.Get()->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    texture2d->GetDesc(&texture2d_desc);

    return texture2d_desc;
}

Microsoft::WRL::ComPtr<ID3DBlob> 
ResourceManager::LoadShaderBytecode(const std::wstring& filename)
{
	// --- キャッシュ確認 ---
	auto it = shader_bytecoodes_.find(filename);
    if (it != shader_bytecoodes_.end())
    {
        return it->second;
    }

	Microsoft::WRL::ComPtr<ID3DBlob> blob;

	const HRESULT hr = D3DReadFileToBlob(filename.c_str(), blob.GetAddressOf());

    if (FAILED(hr))
    {
        std::wstring message =
            L"Failed to load shader bytecode: " + filename +
            L" Error: " + HRTrace(hr);

        OutputDebugStringW(message.c_str());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        return nullptr;
    }

	shader_bytecoodes_.emplace(filename, blob);
    return blob;
}

Microsoft::WRL::ComPtr<ID3D12Resource> ResourceManager::LoadTextureDX12(ID3D12Device*device,ID3D12CommandQueue*queue,const std::wstring& filename)
{
    //キャッシュ確認
    auto it = dx12_textures_.find(filename);
    if (it != dx12_textures_.end())
    {
        return it->second;
    }

    if (device == nullptr )
    {
        _ASSERT_EXPR(false, L"DX12 device is nullptr");
        return nullptr;
    }
    if (queue == nullptr)
    {
        _ASSERT_EXPR(false, L"DX12 queue is nullptr");
        return nullptr;
    }

    //DirectXTexでCPU画像を読み込む
    DirectX::ScratchImage image;
    DirectX::TexMetadata metadata{};
    HRESULT hr{ S_OK };

    std::filesystem::path dds_filename(filename);
    dds_filename.replace_extension("dds");

    //========================================================
    //  DDS かつ Texture3D かどうかを事前に見極める
    //========================================================
    if (std::filesystem::exists(dds_filename))
    {
        //後で実装
        return nullptr;
    }

    //DirectXTex  でCPU画像を読み込む
    hr = DirectX::LoadFromWICFile(
        filename.c_str(),
        DirectX::WIC_FLAGS_NONE,
        &metadata,
        image
    );
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    //2dテクスチャの場合
    if (metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D)
    {
        const DirectX::Image* source_image = image.GetImage(0, 0, 0);

        if (source_image == nullptr)
        {
            _ASSERT_EXPR(false, L"CPU image data is null");
            return nullptr;
        }

        //画像の幅、高さ、形式からDX12テクスチャを作る
        D3D12_RESOURCE_DESC texture_desc{};
        texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture_desc.Width = metadata.width;
        texture_desc.Height = static_cast<UINT>(metadata.height);
        texture_desc.DepthOrArraySize = 1;
        texture_desc.Format = metadata.format;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        D3D12_HEAP_PROPERTIES default_heap{};
        default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        default_heap.CreationNodeMask = 1;
        default_heap.VisibleNodeMask = 1;

        Microsoft::WRL::ComPtr<ID3D12Resource>texture;
        hr = device->CreateCommittedResource(
            &default_heap,
            D3D12_HEAP_FLAG_NONE,
            &texture_desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(texture.GetAddressOf())
        );
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        if(FAILED(hr))
        {
            return nullptr;
        }

        //DX12が要求するコピー先の行ピッチとバッファサイズを取得
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT num_rows = 0;
        uint64_t row_size = 0;
        uint64_t upload_size = 0;

        device->GetCopyableFootprints(
            &texture_desc,
            0, 1, 0,
            &footprint,
            &num_rows,
            &row_size,
            &upload_size
        );

        //GPUが読む用の一時アップロードバッファ
        D3D12_HEAP_PROPERTIES upload_heap{};
        upload_heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        upload_heap.CreationNodeMask = 1;
        upload_heap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC upload_desc{};
        upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        upload_desc.Width = upload_size;
        upload_desc.Height = 1;
        upload_desc.DepthOrArraySize = 1;
        upload_desc.MipLevels = 1;
        upload_desc.SampleDesc.Count = 1;
        upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Microsoft::WRL::ComPtr<ID3D12Resource>upload_buffer;
        hr = device->CreateCommittedResource(
            &upload_heap,
            D3D12_HEAP_FLAG_NONE,
            &upload_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(upload_buffer.GetAddressOf())
        );
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        if (FAILED(hr))
        {
            return nullptr;
        }


        //CPU画像をアップロードバッファへコピーする
        void* mapped_data = nullptr;
        D3D12_RANGE no_read_range{ 0,0 };

        hr = upload_buffer->Map(0, &no_read_range, &mapped_data);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        auto* destination_bytes =
            static_cast<std::uint8_t*>(mapped_data);
        const auto* source_bytes = source_image->pixels;

        //?
        for (UINT row = 0;row < num_rows;++row)
        {
            std::memcpy(
                destination_bytes
                + footprint.Offset
                + static_cast<UINT64>(row) * footprint.Footprint.RowPitch,
                source_bytes
                + static_cast<size_t>(row) * source_image->rowPitch,
                static_cast<size_t>(row_size)
            );
        }

        D3D12_RANGE written_range{
            0,static_cast<size_t>(upload_size)
        };
        upload_buffer->Unmap(0, &written_range);

        //フレーム描画用とは別の、一時アップロード用リストを作る
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>allocator;
        hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(allocator.GetAddressOf())
        );
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>command_list;
        hr = device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(),
            nullptr,
            IID_PPV_ARGS(command_list.GetAddressOf())
        );
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        //アップロードバッファからテクスチャへコピー
        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = texture.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = upload_buffer.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = footprint;

        command_list->CopyTextureRegion(
            &destination,
            0, 0, 0,
            &source,
            nullptr
        );

        //コピー先を、ピクセルシェーダーが読める状態へ遷移
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = texture.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        command_list->ResourceBarrier(1, &barrier);
        hr = command_list->Close();
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        //コピー命令をGPUへ送信
        ID3D12CommandList* command_lists[] = { command_list.Get() };
        queue->ExecuteCommandLists(1, command_lists);

        //子の読み込み処理専用のフェンスでコピー完了を待つ
        Microsoft::WRL::ComPtr<ID3D12Fence>fence;
        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf()));
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        HANDLE fence_event =
            CreateEvent(nullptr, FALSE, FALSE, nullptr);

        _ASSERT_EXPR(fence_event != nullptr, L"CreateEvent failed");
        if (fence_event == nullptr)
        {
            return nullptr;
        }

        hr = queue->Signal(fence.Get(), 1);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        if (SUCCEEDED(hr))
        {
            hr = fence->SetEventOnCompletion(1, fence_event);
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }

        if (SUCCEEDED(hr))
        {
            WaitForSingleObject(fence_event, INFINITE);
        }

        CloseHandle(fence_event);

        if (FAILED(hr))
        {
            return nullptr;
        }

        // GPUで使える状態になったリソースをキャッシュする
        dx12_textures_.emplace(filename, texture);
        return texture;
    }
}
