#ifndef PART2_TEXTURE_H
#define PART2_TEXTURE_H

#include<d3d11.h>
#include<wrl.h>

#include<string>
#include<unordered_map>
#include<mutex>

class LoadTexture
{
public:
    static std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>textures_;
    static std::mutex mutex_;

    static HRESULT LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename, ID3D11ShaderResourceView** shader_resource_view, D3D11_TEXTURE2D_DESC* texture2d_desc);

    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> MakeDummyTexture(ID3D11Device* device, DWORD value/*0xAABBGGRR*/, UINT dimension);

    static void ReleaseAllTexture();

    static D3D11_TEXTURE2D_DESC Texture2dDesc(ID3D11ShaderResourceView* shader_resource_view);

    static HRESULT LoadTextureFromMemory(ID3D11Device* device, const void* data, size_t size, ID3D11ShaderResourceView** shader_resource_view);

};
#endif // !PART2_TEXTURE_H