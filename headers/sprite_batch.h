#ifndef PART2_SPRITE_BATCH_H
#define PART2_SPRITE_BATCH_H

#include<d3d11.h>
#include<directxmath.h>
#include<wrl.h>

#include<vector>

class SpriteBatch
{
public:
    SpriteBatch(ID3D11Device* device, const wchar_t* filename, size_t maxSprites, char* psFile = nullptr);
    ~SpriteBatch();

    void begin(ID3D11DeviceContext* immediate_context,int setPs=0);
    void end(ID3D11DeviceContext* immediate_context);

    void Render(ID3D11DeviceContext* immediate_context,
        float dx, float dy,
        float dw, float dh,
        float angle,
        float red, float green, float blue, float alpha,
        float sx, float sy, float sw, float sh
    );
    void Render(ID3D11DeviceContext* immediate_context,
        float dx, float dy,
        float dw, float dh,
        float angle = 0.f
    )
    {
        Render(immediate_context,
            dx, dy, dw, dh,
            angle,
            1.f, 1.f, 1.f, 1.f,
            0.f, 0.f, static_cast<float>(texture2d_desc_.Width), static_cast<float>(texture2d_desc_.Height));
    }
private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader>      vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       pixel_shader_[3];
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       input_layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>shader_resource_view_;
    D3D11_TEXTURE2D_DESC    texture2d_desc_;

    const size_t max_vertices_;
    
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 texcoord;
    };
    std::vector<Vertex> vertices_;
};
#endif // !PART2_SPRITE_BATCH_H