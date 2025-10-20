#include "../../headers/system/sprite_render_system.h"
#include"../../headers/misc.h"
#include"../../headers/graphics.h"
#include"../../headers/render_state.h"
#include"../../headers/resource_manager.h"

SpriteRenderSystem::SpriteRenderSystem(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
    ID3D11Device*device= Graphics::Instance().GetDevice();

    //頂点バッファ生成
    VertexBuffer vertices[] =
    {
        // 三角形1
        {{-0.5f,  0.5f, 0.0f}, {1,1,1,1}, {1.0f, 0.0f}}, // 左上
        {{ 0.5f,  0.5f, 0.0f}, {1,1,1,1}, {0.0f, 0.0f}}, // 右上
        {{-0.5f, -0.5f, 0.0f}, {1,1,1,1}, {1.0f, 1.0f}}, // 左下

        // 三角形2
        {{ 0.5f,  0.5f, 0.0f}, {1,1,1,1}, {0.0f, 0.0f}}, // 右上
        {{ 0.5f, -0.5f, 0.0f}, {1,1,1,1}, {0.0f, 1.0f}}, // 右下
        {{-0.5f, -0.5f, 0.0f}, {1,1,1,1}, {1.0f, 1.0f}}, // 左下
    };


    D3D11_BUFFER_DESC vb_desc{};
    vb_desc.Usage = D3D11_USAGE_DYNAMIC;
    vb_desc.ByteWidth = sizeof(vertices);
    vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vb_desc.MiscFlags = 0;
    vb_desc.StructureByteStride = 0;
    D3D11_SUBRESOURCE_DATA vb_data{};
    vb_data.pSysMem = vertices;

    HRESULT hr{ S_OK };
    hr = device->CreateBuffer(&vb_desc, &vb_data, vertex_buffer_.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    D3D11_INPUT_ELEMENT_DESC input_element_desc[] =
    {
        // 頂点バッファ（スロット0）
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },

        // インスタンスバッファ（スロット1）
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, sizeof(float) * 4, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, sizeof(float) * 8, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, sizeof(float) * 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
    default_vs_ = ResourceManager::Instance().LoadVertexShader(device, L".\\resources\\shader\\sprite_batch_vs.cso"
    ,default_input_.ReleaseAndGetAddressOf(),input_element_desc,_countof(input_element_desc));
    default_ps_ = ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\sprite_batch_ps.cso");

}

SpriteRenderSystem::~SpriteRenderSystem()
{
    instance_buffer_pool_.clear();
}

void SpriteRenderSystem::Render()
{
    std::unordered_map<ID3D11ShaderResourceView*, std::vector<DirectX::XMFLOAT4X4>> texture_to_worlds;


    comp_mng_.ForEach<ComponentTexture>([&](uint32_t entity_id, ComponentTexture& tex)
        {
            auto* l2w = comp_mng_.TryGetByEntity<ComponentLocalToWorld>(entity_id);
            //auto* texture = comp_mng_.TryGetByEntity<ComponentInstanced>(entity_id);

            if (l2w &&  tex.texture)
            {
                texture_to_worlds[tex.texture.Get()].push_back(l2w->value);
            }
        });

    ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();
    context->VSSetShader(default_vs_.Get(), nullptr, 0);
    context->PSSetShader(default_ps_.Get(), nullptr, 0);

    for (auto& [texture, world_matrices] : texture_to_worlds)
    {
        if (world_matrices.empty()) continue;

        //一応
        if (!texture)continue;

        BatchBufferInfo& buf_info = instance_buffer_pool_[texture];

        const UINT required_size = sizeof(DirectX::XMFLOAT4X4) * static_cast<UINT>(world_matrices.size());

        if (!buf_info.buffer || buf_info.cepacity < world_matrices.size())
        {
            D3D11_BUFFER_DESC desc{};
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth = std::max(required_size, 16u);
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            ID3D11Device* device = Graphics::Instance().GetDevice();
            HRESULT hr = device->CreateBuffer(&desc, nullptr, buf_info.buffer.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), L"インスタンスバッファの作成に失敗しました");
            buf_info.cepacity = world_matrices.size();
        }

        D3D11_MAPPED_SUBRESOURCE mapped{};
        context->Map(buf_info.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, world_matrices.data(), required_size);
        context->Unmap(buf_info.buffer.Get(), 0);

        // テクスチャをセット
        context->PSSetShaderResources(0, 1, &texture);

        UINT strides[2]{ sizeof(VertexBuffer),sizeof(DirectX::XMFLOAT4X4) };
        UINT offsets[2]{ 0,0 };
        ID3D11Buffer* buffers[2]{ vertex_buffer_.Get(),buf_info.buffer.Get() };

        context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
        context->IASetInputLayout(default_input_.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



        // 描画呼び出し
        context->DrawInstanced(
            6,
            static_cast<UINT>(world_matrices.size()),
            0,0);
    }

    context->VSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);

    texture_to_worlds.clear();
}
