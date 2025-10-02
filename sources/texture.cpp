#include"../headers/texture.h"

#include<memory>
#include<filesystem>
#include<WICTextureLoader.h>
#include<DDSTextureLoader.h>

#include"../headers/misc.h"

std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>LoadTexture::textures_;
std::mutex LoadTexture::mutex_;

HRESULT LoadTexture::LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename, ID3D11ShaderResourceView** shader_resource_view, D3D11_TEXTURE2D_DESC* texture2d_desc)
{
    HRESULT hr{ S_OK };
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;

    auto it = textures_.find(filename);
    if (it != textures_.end())
    {
        *shader_resource_view = it->second.Get();
        (*shader_resource_view)->AddRef();
        (*shader_resource_view)->GetResource(resource.GetAddressOf());
    }
    else
    {
        // UNIT.31
        std::filesystem::path dds_filename(filename);
        dds_filename.replace_extension("dds");
        if (std::filesystem::exists(dds_filename.c_str()))
        {
            
            hr = DirectX::CreateDDSTextureFromFile(device, dds_filename.c_str(), resource.GetAddressOf(), shader_resource_view);
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }
        else
        {
            hr = DirectX::CreateWICTextureFromFile(device, filename, resource.GetAddressOf(), shader_resource_view);
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }
        textures_.insert(std::make_pair(filename, *shader_resource_view));
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
    hr = resource.Get()->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    texture2d->GetDesc(texture2d_desc);

    return hr;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTexture::MakeDummyTexture(ID3D11Device* device, DWORD value, UINT dimension)
{
    std::wstringstream name;
    name << std::setw(8) << std::setfill(L'0') << std::hex << std::uppercase << value << L"." << dimension;

    if (textures_.find(name.str().c_str()) != textures_.end())
    {
        return textures_.at(name.str().c_str());
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

    std::lock_guard<std::mutex> lock(mutex_);
    textures_.emplace(std::make_pair(name.str().c_str(), shader_resource_view));

    return shader_resource_view;
}

inline void LoadTexture::ReleaseAllTexture()
{
    std::lock_guard<std::mutex> lock(mutex_);
    textures_.clear();
}

 D3D11_TEXTURE2D_DESC LoadTexture::Texture2dDesc(ID3D11ShaderResourceView* shader_resource_view)
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

 HRESULT LoadTexture::LoadTextureFromMemory(ID3D11Device* device, const void* data, size_t size, ID3D11ShaderResourceView** shader_resource_view)
 {
     HRESULT hr{ S_OK };
     Microsoft::WRL::ComPtr<ID3D11Resource> resource;

     hr = DirectX::CreateDDSTextureFromMemory(device, reinterpret_cast<const uint8_t*>(data), size, resource.GetAddressOf(), shader_resource_view);
     if (hr != S_OK)
     {
         hr = DirectX::CreateWICTextureFromMemory(device, reinterpret_cast<const uint8_t*>(data), size, resource.GetAddressOf(), shader_resource_view);
         _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
     }

     return hr;
 }
