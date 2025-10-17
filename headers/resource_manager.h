#pragma once

#include<unordered_map>
#include<d3d11.h>
#include<wrl.h>
#include<memory>
#include"gltf_model.h"
#include"texture.h"

//リソースマネージャ
//ものすごくやりたくありませんが、シングルトンパターンにします
class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager() = default;
    // コピー・ムーブを禁止
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;

    static ResourceManager& Instance()
    {
        static ResourceManager instance;
        return instance;
    }

    // GLTFモデルの読み込み（ロード済みなら共有）
    std::shared_ptr<GltfModel> LoadGltfModel(ID3D11Device* device, const std::string& filename);
    //GLTFモデルの取得
    const std::unordered_map<std::string, std::shared_ptr<GltfModel>>& GetGltfs() {
        return gltf_models_;
    }
    
    // シェーダーの取得（必要なら）
    Microsoft::WRL::ComPtr<ID3D11VertexShader> LoadVertexShader(ID3D11Device* device, const std::wstring& filename);
    Microsoft::WRL::ComPtr<ID3D11PixelShader> LoadPixelShader(ID3D11Device* device, const std::wstring& filename);



    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename, D3D11_TEXTURE2D_DESC* texture2d_desc);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> MakeDummyTexture(ID3D11Device* device, DWORD value/*0xAABBGGRR*/, UINT dimension);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTextureFromMemory(ID3D11Device* device, const void* data, size_t size);

    ////バイナリデータからハッシュを計算、文字にする
    std::wstring MakeHashKey(const void* data, size_t size);
    D3D11_TEXTURE2D_DESC Texture2dDesc(ID3D11ShaderResourceView* shader_resource_view);


private:
    std::unordered_map<std::string, std::shared_ptr<GltfModel>> gltf_models_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11VertexShader>> vertex_shaders_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11PixelShader>> pixel_shaders_;

};