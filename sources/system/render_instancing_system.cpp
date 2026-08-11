#include"../../headers/system/render_instancing_system.h"

#include<cassert>
#include<algorithm>
#include<functional>
#include<array>
#include<cmath>

#include"../../headers/graphics.h"
#include"../../headers/gltf_model.h"
#include"../../headers/misc.h"
#include"../../headers/component/component_manager.h"
#include"../../headers/system/render_frustum_helper.h"


InstancingRenderSystem::InstancingRenderSystem(ComponentManager& comp_mng, RenderPass render_pass )
    :comp_mng_(comp_mng)
    ,IRenderSystem(render_pass)
{
}

void InstancingRenderSystem::Render()
{
    //メインのカメラ情報を取得
    ComponentCamera* main_camera = nullptr;
    comp_mng_.ForEach<ComponentCamera>([&](uint32_t entity_id, ComponentCamera& camera)
        {
            if (camera.main_camera_flag_)
            {
                main_camera = &camera;
            }
        });

    // メインカメラが存在しない場合は描画をスキップ
    if (!main_camera) {
        return;
    }

    std::array<DirectX::XMFLOAT4, 6>frustum_planes{};
    FrustumHelper::CreateFrustumPlanesFromMatrix(main_camera->view_projection_transform, frustum_planes);


    // モデルごとにインスタンスをグループ化

    for (auto& [model, worlds] : model_to_worlds_)
    {
        worlds.clear();
    }
    for (auto& [model, animated_nodes_list_] : model_to_animated_nodes_list_)
    {
        animated_nodes_list_.clear();
    }

    comp_mng_.ForEach<
        ComponentInstanced,
        ComponentGltf,
        ComponentLocalToWorld,
        ComponentBoundingBox
    >([&](
        uint32_t entity_id,
        ComponentInstanced& instanced,
        ComponentGltf& gltf,
        ComponentLocalToWorld& l2w,
        ComponentBoundingBox& b_box
        ) {

            if (!gltf.model)
            {
                return;
            }

            bool visible = true;

            // バウンディングボックスを取得して、フラスタムカリングを行う
            if (FrustumHelper::IsValidWorldBoundingBox(b_box))
            {
                visible = FrustumHelper::IsAABBVisibleFromFrustumPlanes(b_box, frustum_planes);
            }
            else
            {
                // バウンディングボックスが無効な場合は、常に描画する
                visible = true;
            }

            if (!visible)
            {
                return;
            }

            GltfModel* model = gltf.model.get();
            model_to_worlds_[model].push_back(l2w.value);
            model_to_animated_nodes_list_[model].push_back(&gltf.animated_nodes);


        }
    );

    ID3D11Device* device = Graphics::Instance().GetDevice();
    ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();
    HRESULT hr{ S_OK };

    for (auto& [model, world_matrices] : model_to_worlds_)
    {
        if (!model||world_matrices.empty()) continue;

        auto& animated_nodes_list = model_to_animated_nodes_list_[model];
        if (animated_nodes_list.size() != world_matrices.size())
        {
            _ASSERT_EXPR(false, L"world_matrices and animated_nodes_list size mismatch");
            continue;
        }

        InstancingRenderSystem::InstanceBufferInfo& buf_info = instance_buffer_pool_[model];

        const UINT required_size = sizeof(DirectX::XMFLOAT4X4) * static_cast<UINT>(world_matrices.size());

        // 必要に応じて再生成（足りないときだけ）
        if (!buf_info.buffer || buf_info.cepasity < world_matrices.size())
        {
            D3D11_BUFFER_DESC desc{};
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth = (std::max)(required_size, 16u);
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            hr = device->CreateBuffer(&desc, nullptr, buf_info.buffer.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), L"インスタンスバッファの作成に失敗しました");
            buf_info.cepasity = world_matrices.size();
        }

        // Map でデータ更新
        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr=context->Map(buf_info.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        if (SUCCEEDED(hr))
        {
            memcpy(mapped.pData, world_matrices.data(), required_size);
            context->Unmap(buf_info.buffer.Get(), 0);
        }

        model->InstancingRender(
            Graphics::Instance().GetDevice(),
            context,
            static_cast<UINT>(world_matrices.size()),
            buf_info.buffer.Get(),
            animated_nodes_list,
            0, false
        );
    }
}