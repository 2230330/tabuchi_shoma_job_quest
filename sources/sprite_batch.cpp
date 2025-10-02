#include "../headers/sprite_batch.h"



#include"../headers/misc.h"
#include"../headers/texture.h"
#include"../headers/shader.h"

SpriteBatch::SpriteBatch(ID3D11Device* device, const wchar_t* filename, size_t maxSprites,char*psFile)
    :max_vertices_(maxSprites * 6)
{
    HRESULT hr{ S_OK };


    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * max_vertices_);
    buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;

    hr = device->CreateBuffer(&buffer_desc, NULL, vertex_buffer_.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    const char* cso_name{ ".\\resources\\shader\\sprite_vs.cso" };
    D3D11_INPUT_ELEMENT_DESC input_element_desc[]
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
    };
    shader_from_cso::CreateVsFromCso(device, cso_name, vertex_shader_.GetAddressOf(),
        input_layout_.GetAddressOf(), input_element_desc, _countof(input_element_desc));

    if (psFile!=nullptr)shader_from_cso::CreatePsFromCso(device, psFile, pixel_shader_[1].GetAddressOf());
    cso_name = ".\\resources\\shader\\sprite_ps.cso";
    shader_from_cso::CreatePsFromCso(device,cso_name,pixel_shader_[0].GetAddressOf());

    LoadTexture::LoadTextureFromFile(device, filename, shader_resource_view_.GetAddressOf(),&texture2d_desc_);
    texture2d_desc_ = LoadTexture::Texture2dDesc(shader_resource_view_.Get());
}

SpriteBatch::~SpriteBatch()
{
}

void SpriteBatch::begin(ID3D11DeviceContext* immediate_context,int setPs)
{
    vertices_.clear();
    immediate_context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
    if(pixel_shader_[setPs])immediate_context->PSSetShader(pixel_shader_[setPs].Get(), nullptr, 0);
    else immediate_context->PSSetShader(pixel_shader_[0].Get(), nullptr, 0);
    if (shader_resource_view_.Get() != nullptr)
        immediate_context->PSSetShaderResources(0, 1, shader_resource_view_.GetAddressOf());
}

void SpriteBatch::end(ID3D11DeviceContext* immediate_context)
{
    HRESULT hr{ S_OK };
    D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
    hr = immediate_context->Map(vertex_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    size_t vertex_count{ vertices_.size() };
    _ASSERT_EXPR(max_vertices_>=vertex_count,"Buffer overflow");
    Vertex* data{ reinterpret_cast<Vertex*>(mapped_subresource.pData) };
    if (data != nullptr)
    {
        const Vertex* p = vertices_.data();
        memcpy_s(data, max_vertices_ * sizeof(Vertex), p, vertex_count * sizeof(Vertex));
    }
    immediate_context->Unmap(vertex_buffer_.Get(), 0);

    UINT stride{ sizeof(Vertex) };
    UINT offset{ 0 };
    immediate_context->IASetVertexBuffers(0, 1, vertex_buffer_.GetAddressOf(), &stride, &offset);
    immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    immediate_context->IASetInputLayout(input_layout_.Get());

    immediate_context->DrawInstanced(static_cast<UINT>(vertex_count), 1,0,0);

    vertices_.clear();
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

void SpriteBatch::Render (ID3D11DeviceContext* immediate_context,
    float dx, float dy,
    float dw, float dh,
    float angle,
    float red, float green, float blue, float alpha,
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

    float u0{ sx / texture2d_desc_.Width };
    float v0{ sy / texture2d_desc_.Height };
    float u1{ (sx + sw) / texture2d_desc_.Width };
    float v1{ (sy + sh) / texture2d_desc_.Height };

    Vertex set_vertex;
    set_vertex={ {x0,y0,0},{red,green,blue,alpha},{u0,v0} };
    vertices_.emplace_back(set_vertex);
    set_vertex={ {x1,y1,0},{red,green,blue,alpha},{u1,v0} };
    vertices_.emplace_back(set_vertex);
    set_vertex={ {x2,y2,0},{red,green,blue,alpha},{u0,v1} };
    vertices_.emplace_back(set_vertex);
    set_vertex={ {x2,y2,0},{red,green,blue,alpha},{u0,v1} };
    vertices_.emplace_back(set_vertex);
    set_vertex={ {x1,y1,0},{red,green,blue,alpha},{u1,v0} };
    vertices_.emplace_back(set_vertex);
    set_vertex={ {x3,y3,0},{red,green,blue,alpha},{u1,v1} };
    vertices_.emplace_back(set_vertex);

}
