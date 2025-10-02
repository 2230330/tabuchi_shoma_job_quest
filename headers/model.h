#ifndef PART2_MODEL_H
#define PART2_MODEL_H

#include<string>
#include<d3d11.h>
#include<DirectXMath.h>

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 texcoord;
};

//objファイル読み込み可能
class Model
{
public:
    Model() = default;
    ~Model() = default;

    bool LoadFromFile(const std::string& filename);
    void Render(
        ID3D11DeviceContext* immediate_context,
        DirectX::XMFLOAT4X4& world,
        DirectX::XMFLOAT4& color
    );

private:

};

#endif // !PART2_MODEL_H
