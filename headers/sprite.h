#ifndef PART2_SPRITE_H
#define PART2_SPRITE_H

#include<d3d11.h>
#include<directxmath.h>
#include<wrl.h>

#include<string>

class Sprite
{
public:
    Sprite(ID3D11Device* device,const wchar_t*filename);
    ~Sprite();

    void Render(ID3D11DeviceContext* immediate_context,
        float dx, float dy,
        float dw, float dh,
        float angle,
        float red, float green, float blue, float alpha,
        float sx,float sy,float sw,float sh
    );

    void Render(ID3D11DeviceContext* immediate_context,
        float dx,float dy,
        float dw,float dh,
        float angle=0.f
        )
    {
        Render(immediate_context,
            dx, dy, dw, dh,
            angle, 
            1.f, 1.f, 1.f, 1.f,
            0.f, 0.f, static_cast<float>(texture2d_desc_.Width), static_cast<float>(texture2d_desc_.Height));
    }

    void TextOut(ID3D11DeviceContext* immediate_context, std::string s,
        float x, float y, float w, float h, float r=1.f, float g=1.f, float b=1.f, float a=1.f);

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader>      vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       input_layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>shader_resource_view_;
    D3D11_TEXTURE2D_DESC    texture2d_desc_;

    struct Vertex_
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 texcoord;
    };
};

#endif // !PART2_SPRITE_H