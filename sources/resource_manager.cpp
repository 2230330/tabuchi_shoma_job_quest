#include"../headers/resource_manager.h"
#include<d3dcompiler.h>
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

