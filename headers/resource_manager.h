#pragma once

#include<unordered_map>
#include<d3d11.h>
#include<wrl.h>
#include<memory>
#include"gltf_model.h"


class ResourceManager {
public:
    // GLTFモデルの読み込み（ロード済みなら共有）
    std::shared_ptr<GltfModel> LoadGltfModel(ID3D11Device* device, const std::string& filename);
    //GLTFモデルの取得
    const std::unordered_map<std::string, std::shared_ptr<GltfModel>>& GetGltfs() {
        return gltf_models_;
    }
    
    // シェーダーの取得（必要なら）
    Microsoft::WRL::ComPtr<ID3D11VertexShader> LoadVertexShader(ID3D11Device* device, const std::wstring& filename);
    Microsoft::WRL::ComPtr<ID3D11PixelShader> LoadPixelShader(ID3D11Device* device, const std::wstring& filename);



private:
    std::unordered_map<std::string, std::shared_ptr<GltfModel>> gltf_models_;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11VertexShader>> vertex_shaders_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11PixelShader>> pixel_shaders_;

};