#include"../../headers/system/instancing_render_system.h"

#include<cassert>
#include<algorithm>
#include<functional>

#include"../../headers/graphics.h"

InstancingRenderSystem::InstancingRenderSystem(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
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

        InstancingRenderSystem::InstanceBufferInfo& buf_info = instance_buffer_pool_[model];

        const UINT required_size = sizeof(XMFLOAT4X4) * static_cast<UINT>(world_matrices.size());

        // 必要に応じて再生成（足りないときだけ）
        if (!buf_info.buffer || buf_info.cepasity < world_matrices.size())
        {
            D3D11_BUFFER_DESC desc{};
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth = std::max(required_size, 16u);
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            HRESULT hr = device->CreateBuffer(&desc, nullptr, buf_info.buffer.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), L"インスタンスバッファの作成に失敗しました");
            buf_info.cepasity = world_matrices.size();
        }

        // Map でデータ更新
        D3D11_MAPPED_SUBRESOURCE mapped{};
        context->Map(buf_info.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, world_matrices.data(), required_size);
        context->Unmap(buf_info.buffer.Get(), 0);

        // インスタンシング描画呼び出し
        model->InstancingRender(context,
            static_cast<UINT>(world_matrices.size()),
            buf_info.buffer.Get(),
            0);
    }
}