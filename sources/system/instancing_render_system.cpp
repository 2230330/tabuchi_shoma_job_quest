#include"../../headers/system/instancing_render_system.h"

#include<cassert>
#include<algorithm>
#include<functional>

#include"../../headers/graphics.h"

InstancingRenderSystem::InstancingRenderSystem(ComponentManager& comp_mng, World& world)
    :comp_mng_(comp_mng)
    ,world_(world)
{
}

void InstancingRenderSystem::Render()
{
    using namespace DirectX;

    // モデルごとにインスタンスをグループ化
    std::unordered_map<GltfModel*, std::vector<XMFLOAT4X4>> model_to_worlds;

    comp_mng_.ForEach<ComponentGltf>([&](uint32_t entity_id, ComponentGltf& gltf)
        {
            auto* l2w = comp_mng_.TryGetByEntity<ComponentLocalToWorld>(entity_id);
            auto* instanced = comp_mng_.TryGetByEntity<ComponentInstanced>(entity_id);

            // インスタンシング対象のみ抽出
            if (l2w && instanced)
            {
                model_to_worlds[gltf.model.get()].push_back(l2w->value);
            }
        });

    ID3D11Device* device = Graphics::Instance().GetDevice();
    ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

    for (auto& [model, world_matrices] : model_to_worlds)
    {
        if (world_matrices.empty()) continue;

        // インスタンス用ワールド行列バッファ作成
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.ByteWidth =((sizeof(XMFLOAT4X4) * (static_cast<UINT>(world_matrices.size()))));
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA init_data = {};
        init_data.pSysMem = world_matrices.data();

        Microsoft::WRL::ComPtr<ID3D11Buffer> instance_buffer;
        HRESULT hr = device->CreateBuffer(&desc, &init_data, instance_buffer.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), L"インスタンスバッファの作成に失敗しました");

        // インスタンシング描画呼び出し
        model->InstancingRender(context,
            static_cast<UINT>(world_matrices.size()),
            instance_buffer.Get(),
            0);
    }
}

