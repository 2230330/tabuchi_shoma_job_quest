#ifndef PART2_STATIC_MESH_H
#define PART2_STATIC_MESH_H

#include<d3d11.h>
#include<DirectXMath.h>
#include<wrl.h>

#include<vector>
#include<fstream>

//OBJファイル形式のオブジェクト専用描画クラス
class StaticMesh
{
public:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 texcoord;
    };
    struct InstanceData
    {
        DirectX::XMFLOAT4X4 instance_world;
        DirectX::XMFLOAT4   instance_color;
    };
    struct Subset
    {
        std::wstring usemtl;
        std::uint32_t index_start{ 0 };
        std::uint32_t index_count{ 0 };
    };
    struct Material
    {
        std::wstring name;
        DirectX::XMFLOAT4 Ka{ 0.2f,0.2f,0.2f,1.0f };
        DirectX::XMFLOAT4 Kd{ 0.8f,0.8f,0.8f,1.0f };
        DirectX::XMFLOAT4 Ks{ 1.0f,1.0f,1.0f,1.0f };
        std::wstring texture_filenames[2];
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_views[2];
    };
private:
    //バッファ
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> instance_buffer_;

    //シェーダー
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  input_layout_;

    //配列
    UINT indices_count_;
    std::vector<InstanceData> instances_;
    std::vector<Subset>       subsets_;
    std::vector<Material>     materials_;
public:
    StaticMesh(ID3D11Device* device, const wchar_t* obj_filename, bool uv_flag = false, UINT count = 1);
    virtual ~StaticMesh() = default;
    //描画用関数
    void Render(
        ID3D11DeviceContext* immediate_context,
        const DirectX::XMFLOAT4X4& world,
        const DirectX::XMFLOAT4& color,
        const UINT &instance_number = {}
    );

private:
    //頂点バッファとインデックスバッファの設定
    void __cdecl CreateComBuffers(ID3D11Device* device, const Vertex* vertices, const UINT vertices_count,
        const uint32_t* indices, const UINT indices_count);
    //インスタンス用バッファの更新
    void UpdateInstanceBuffer(ID3D11DeviceContext* immediate_context,const std::vector<InstanceData>&instance);
};

#endif // !PART2_STATIC_MESH_H
