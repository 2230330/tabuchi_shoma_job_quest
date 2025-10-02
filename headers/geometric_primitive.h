#ifndef PART2_GEOMETRIC_PRIMITIVE_H
#define PART2_GEOMETRIC_PRIMITIVE_H

#include<d3d11.h>
#include<DirectXMath.h>
#include<wrl.h>
#include<vector>

class GeometricPrimitive
{
public:
    struct Vertex
    {
        DirectX::XMFLOAT3       position;
        DirectX::XMFLOAT3       normal;
    };
    //インスタンス用構造体
    struct InstanceData
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4   color;
    };


    UINT indices_count_;
    std::vector<InstanceData>instances_;
protected:
    //バッファ
    Microsoft::WRL::ComPtr<ID3D11Buffer>        vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        index_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        instance_buffer_;  
    //シェーダー用
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   input_layout_;

    //XMFLOAT3 scale_;
    //XMFLOAT3 rotation_;
    //XMFLOAT3 transform_;
public:
    GeometricPrimitive(ID3D11Device* device) { indices_count_ = 0; }
    virtual ~GeometricPrimitive() = default;

    void  Render(ID3D11DeviceContext* immediate_context,
         const DirectX::XMFLOAT4& color);

    //新しい物体を出す
    void CreateGeometricPrimitive(
        DirectX::XMFLOAT3 scale,
        DirectX::XMFLOAT3 rotation, 
        DirectX::XMFLOAT3 position,
        DirectX::XMFLOAT4 color={1,1,1,1});

    //void SetScale(float scale_x, float scale_y, float scale_z)
    //{
    //    this->scale_ = { scale_x, scale_y, scale_z };
    //}
    //void SetScale(XMFLOAT3 scale) 
    //{
    //    SetScale(scale.x, scale.y, scale.z);
    //}
    //void SetRotation(float rotation_x, float rotation_y, float rotation_z)
    //{
    //    this->rotation_ = { rotation_x, rotation_y, rotation_z };
    //}
    //void SetRotation(XMFLOAT3 rotation)
    //{
    //    SetRotation(rotation.x, rotation.y, rotation.z);
    //}
    //void SetTransform(XMFLOAT3 transform)
    //{
    //    SetTransform(transform.x, transform.y, transform.z);
    //}
    //void SetTransform(float transform_x, float transform_y, float transform_z)
    //{
    //    this->transform_ = { transform_x, transform_y, transform_z };
    //}

    //XMFLOAT3 GetScale() { return scale_; }
    //XMFLOAT3 GetRotation() { return rotation_; }
    //XMFLOAT3 GetTransform() { return transform_; }

protected:
    //頂点バッファとインデックスバッファの設定
    void __cdecl CreateComBuffers(ID3D11Device* device,const Vertex* vertices,const UINT vertices_count,
        const uint32_t* indices,const UINT indices_count);
    //インスタンス用バッファの設定
    void CreateInstanceBuffer(ID3D11Device* device);
    //インスタンス用のバッファの更新
    void UpdateInstances(
        ID3D11DeviceContext* immediate_context,
        const std::vector<InstanceData>& instances);
};

class GeometricCube : public GeometricPrimitive
{
public:
    GeometricCube(ID3D11Device* device);
};
class GeometricSphere :public GeometricPrimitive
{
public:
    GeometricSphere(
        ID3D11Device* device,
        float radius,
        unsigned int longitudeSegments,
        unsigned int latitudeSegments
    );
};

class GeometricCylinder :public GeometricPrimitive
{
public:
    GeometricCylinder(
        ID3D11Device* device, 
        float radius,
        float height,
        unsigned int sliceCount,
        unsigned int stackCount
    );
};

class GeometricTorus :public GeometricPrimitive
{
public:
    GeometricTorus(
        ID3D11Device* device,
        float outerRadius,
        float innerRadius,
        UINT sliceCount,
        UINT stackCount
    );
};

#endif // !PART2_GEOMETRIC_PRIMITIVE_H