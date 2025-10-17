#include"../headers/resource_manager.h"
#include<d3dcompiler.h>
#include<DDSTextureLoader.h>
#include<WICTextureLoader.h>
#include<functional>
#include<filesystem>
#include<iostream>

#include"../headers/texture.h"
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

Microsoft::WRL::ComPtr<ID3D11VertexShader> ResourceManager::LoadVertexShader(ID3D11Device* device, const std::wstring& filename)
{
    auto it = vertex_shaders_.find(filename);
    if (it != vertex_shaders_.end())
    {
        return it->second;
    }

    Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
    Microsoft::WRL::ComPtr<ID3DBlob> blob;

    HRESULT hr = D3DReadFileToBlob(filename.c_str(), blob.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader.GetAddressOf());
    }

    if (FAILED(hr))
    {
        return nullptr;
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
    }

    if (FAILED(hr))
    {
        return nullptr;
    }

    pixel_shaders_.emplace(filename, shader);
    return shader;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ResourceManager::LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename, D3D11_TEXTURE2D_DESC* texture2d_desc)
{
        HRESULT hr{ S_OK };
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>shader_resource_view;

        auto it = textures_.find(filename);
        if (it != textures_.end())
        {
            shader_resource_view = it->second.Get();
            (shader_resource_view)->AddRef();
            (shader_resource_view)->GetResource(resource.GetAddressOf());
        }
        else
        {
            // UNIT.31
            std::filesystem::path dds_filename(filename);
            dds_filename.replace_extension("dds");
            if (std::filesystem::exists(dds_filename.c_str()))
            {
                
                hr = DirectX::CreateDDSTextureFromFile(device, dds_filename.c_str(), resource.GetAddressOf(), shader_resource_view.GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
            }
            else
            {
                hr = DirectX::CreateWICTextureFromFile(device, filename, resource.GetAddressOf(), shader_resource_view.GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
            }
            textures_.insert(std::make_pair(filename, shader_resource_view));
        }
    
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
        hr = resource.Get()->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        texture2d->GetDesc(texture2d_desc);
    
        return shader_resource_view;
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