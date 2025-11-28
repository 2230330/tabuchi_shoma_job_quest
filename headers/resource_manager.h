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
    //シングルトンパターンにしたので、消去のタイミングをこちらで制御したいので実装しました。
    void Shutdown()
    {
        gltf_models_.clear();
        textures_.clear();
        vertex_shaders_.clear();
        pixel_shaders_.clear();
        compute_shaders_.clear();
    }

    // GLTFモデルの読み込み（ロード済みなら共有）
    std::shared_ptr<GltfModel> LoadGltfModel(ID3D11Device* device, const std::string& filename);
    //GLTFモデルの取得
    const std::unordered_map<std::string, std::shared_ptr<GltfModel>>& GetGltfs() {
        return gltf_models_;
    }
    
    // シェーダーの取得（必要なら）
    Microsoft::WRL::ComPtr<ID3D11VertexShader> LoadVertexShader(ID3D11Device* device, const std::wstring& filename,
        ID3D11InputLayout**input_layout=nullptr,D3D11_INPUT_ELEMENT_DESC*input_element_desc=nullptr,UINT num_element=0);
    Microsoft::WRL::ComPtr<ID3D11PixelShader> LoadPixelShader(ID3D11Device* device, const std::wstring& filename);
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> LoadComputeShader(ID3D11Device* device, const std::wstring& filename);


    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> MakeDummyTexture(ID3D11Device* device, DWORD value/*0xAABBGGRR*/, UINT dimension);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTextureFromMemory(ID3D11Device* device, const void* data, size_t size);
    //テクスチャの取得
    const std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>GetTextures() {
        return textures_;
    }

    ////バイナリデータからハッシュを計算、文字にする
    std::wstring MakeHashKey(const void* data, size_t size);
    D3D11_TEXTURE2D_DESC Texture2dDesc(ID3D11ShaderResourceView* shader_resource_view);


private:
    std::unordered_map<std::string, std::shared_ptr<GltfModel>> gltf_models_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11VertexShader>> vertex_shaders_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11PixelShader>> pixel_shaders_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ComputeShader>> compute_shaders_;
};