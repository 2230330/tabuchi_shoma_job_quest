#include "../headers/sprite.h"

#include <WICTextureLoader.h>

#include<sstream>

#include"../headers/misc.h"
#include"../headers/resource_manager.h"
#include"../headers/shader.h"

Sprite::Sprite(ID3D11Device* device,const wchar_t*filename)
{
    HRESULT hr{ S_OK };

    Vertex_ vertices[]
    {
        {{-1.,+1.,0},{1,1,1,1},{0,0}},
        {{+1.,+1.,0},{1,0,0,1},{1,0}},
        {{-1.,-1.,0},{0,1,0,1},{0,1}},
        {{+1.,-1.,0},{0,0,1,1},{1,1}},
    };

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(vertices);
    buffer_desc.Usage =     D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;
    D3D11_SUBRESOURCE_DATA subresource_data{};
    subresource_data.pSysMem = vertices;
    subresource_data.SysMemSlicePitch = 0;
    subresource_data.SysMemSlicePitch = 0;
    hr = device->CreateBuffer(&buffer_desc, &subresource_data, vertex_buffer_.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));



    const char* cso_name{ ".\\shaderes\\sprite_vs.cso" };
    D3D11_INPUT_ELEMENT_DESC input_element_desc[]
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
    };
    shader_from_cso::CreateVsFromCso(device, cso_name, vertex_shader_.GetAddressOf()
        , input_layout_.GetAddressOf(), input_element_desc, _countof(input_element_desc));

    cso_name = ".\\shaderes\\sprite_ps.cso";
    shader_from_cso::CreatePsFromCso(device, cso_name, pixel_shader_.GetAddressOf());

    if(filename!=nullptr)
    { 
        shader_resource_view_=ResourceManager::Instance().LoadTextureFromFile(device, filename);
        texture2d_desc_ = ResourceManager::Instance().Texture2dDesc(shader_resource_view_.Get());
    }
    
}

inline auto rotate(float& x, float& y, float cx, float cy, float angle)
{
    x -= cx;
    y -= cy;

    float cos(cosf(DirectX::XMConvertToRadians(angle)));
    float sin(sinf(DirectX::XMConvertToRadians(angle)));
    float tx{ x }, ty{ y };
    x = cos * tx + -sin * ty;
    y = sin * tx + cos * ty;

    x += cx;
    y += cy;
}

void Sprite::Render(ID3D11DeviceContext* immediate_context,
                    float dx, float dy,
                    float dw, float dh,
                    float angle,
                    float red,float green,float blue,float alpha,
                    float sx, float sy, float sw, float sh
)
{
    D3D11_VIEWPORT viewport{};
    UINT num_viewports{ 1 };
    immediate_context->RSGetViewports(&num_viewports, &viewport);

    if (sw == FLT_MIN && sh == FLT_MIN)
    {
        sw = static_cast<float>(texture2d_desc_.Width);
        sh = static_cast<float>(texture2d_desc_.Height);
    }

    float x0{ dx };
    float y0{ dy };
    float x1{ dx + dw };
    float y1{ dy };
    float x2{ dx };
    float y2{ dy + dh };
    float x3{ dx + dw };
    float y3{ dy + dh };


    float cx = dx + dw * 0.5f;
    float cy = dy + dh * 0.5f;
    rotate(x0, y0, cx, cy, angle);
    rotate(x1, y1, cx, cy, angle);
    rotate(x2, y2, cx, cy, angle);
    rotate(x3, y3, cx, cy, angle);

    x0 = 2.0f * x0 / viewport.Width - 1.0f;
    y0 = 1.0f - 2.0f * y0 / viewport.Height;
    x1 = 2.0f * x1 / viewport.Width - 1.0f;
    y1 = 1.0f - 2.0f * y1 / viewport.Height;
    x2 = 2.0f * x2 / viewport.Width - 1.0f;
    y2 = 1.0f - 2.0f * y2 / viewport.Height;
    x3 = 2.0f * x3 / viewport.Width - 1.0f;
    y3 = 1.0f - 2.0f * y3 / viewport.Height;

    HRESULT hr{ S_OK };
    D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
    hr = immediate_context->Map(vertex_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    Vertex_* vertices{ reinterpret_cast<Vertex_*>(mapped_subresource.pData) };
    if (vertices != nullptr)
    {
        vertices[0].position = { x0,y0,0 };
        vertices[1].position = { x1,y1,0 };
        vertices[2].position = { x2,y2,0 };
        vertices[3].position = { x3,y3,0 };
        vertices[0].color = vertices[1].color = vertices[2].color = vertices[3].color = { red,green,blue,alpha };

        vertices[0].texcoord = { 0,0 };
        vertices[1].texcoord = { 1,0 };
        vertices[2].texcoord = { 0,1 };
        vertices[3].texcoord = { 1,1 };

        vertices[0].texcoord.x = (std::min)(vertices[0].texcoord.x, 1.0f);
        vertices[0].texcoord.y = (std::min)(vertices[0].texcoord.y, 1.0f);
        vertices[1].texcoord.x = (std::min)(vertices[1].texcoord.x, 1.0f);
        vertices[1].texcoord.y = (std::min)(vertices[1].texcoord.y, 1.0f);
        vertices[2].texcoord.x = (std::min)(vertices[2].texcoord.x, 1.0f);
        vertices[2].texcoord.y = (std::min)(vertices[2].texcoord.y, 1.0f);
        vertices[3].texcoord.x = (std::min)(vertices[3].texcoord.x, 1.0f);
        vertices[3].texcoord.y = (std::min)(vertices[3].texcoord.y, 1.0f);

        vertices[0].texcoord.x = (sx + vertices[0].texcoord.x * sw) / texture2d_desc_.Width;
        vertices[0].texcoord.y = (sy + vertices[0].texcoord.y * sh) / texture2d_desc_.Height;
        vertices[1].texcoord.x = (sx + vertices[1].texcoord.x * sw) / texture2d_desc_.Width;
        vertices[1].texcoord.y = (sy + vertices[1].texcoord.y * sh) / texture2d_desc_.Height;
        vertices[2].texcoord.x = (sx + vertices[2].texcoord.x * sw) / texture2d_desc_.Width;
        vertices[2].texcoord.y = (sy + vertices[2].texcoord.y * sh) / texture2d_desc_.Height;
        vertices[3].texcoord.x = (sx + vertices[3].texcoord.x * sw) / texture2d_desc_.Width;
        vertices[3].texcoord.y = (sy + vertices[3].texcoord.y * sh) / texture2d_desc_.Height;
    }

    immediate_context->Unmap(vertex_buffer_.Get(), 0);

    UINT stride{ sizeof(Vertex_) };
    UINT offset{ 0 };
    immediate_context->IASetVertexBuffers(0, 1, vertex_buffer_.GetAddressOf(), &stride, &offset);
    immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    immediate_context->IASetInputLayout(input_layout_.Get());

    if (shader_resource_view_.Get() != nullptr)immediate_context->PSSetShaderResources(0, 1, shader_resource_view_.GetAddressOf());
    immediate_context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
    immediate_context->PSSetShader(pixel_shader_.Get(), nullptr, 0);

    immediate_context->Draw(4, 0);
}

void Sprite::TextOut(ID3D11DeviceContext* immediate_context, std::string s, float x, float y, float w, float h, float r, float g, float b, float a)
{
    float sw = static_cast<float>(texture2d_desc_.Width / 16.f);
    float sh = static_cast<float>(texture2d_desc_.Height / 16.f);
    float carriage = 0;
    int size = static_cast<int>(sizeof(s) / sizeof(char));
    const char* c = s.data();
    for (const char c : s)
    {
        Render(immediate_context, x + carriage, y, w, h, 0.f, r, g, b, a,
            sw * (c & 0x0F), sh * (c >> 4), sw, sh);
        carriage += w;
    }
}

Sprite::~Sprite()
{
}