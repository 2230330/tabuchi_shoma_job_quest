#include"../../headers/system/update_bounding_box_system.h"

#include<DirectXMath.h>

#include"../../headers/component/component_manager.h"

UpdateBoundingBoxSystem::UpdateBoundingBoxSystem(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{}

//境界ボックスの更新
void UpdateBoundingBoxSystem::Update(float elapsed_time)
{
    comp_mng_.ParallelForEach<
        ComponentBoundingBox,
        ComponentLocalToWorld,
        ComponentGltf
    >(
        [](uint32_t entity_id,
            ComponentBoundingBox& b_box,
            ComponentLocalToWorld& l2w,
            ComponentGltf& gltf)
        {
            // すでにワールド空間のAABBが計算済みで、かつgltfが静的オブジェクトの場合は計算をスキップ
            if (gltf.dirty && b_box.world_max.x != -FLT_MAX)
                return;

            DirectX::XMFLOAT3 center =
            {
                (b_box.local_min.x + b_box.local_max.x) * 0.5f,
                (b_box.local_min.y + b_box.local_max.y) * 0.5f,
                (b_box.local_min.z + b_box.local_max.z) * 0.5f
            };

            //ローカルExtents(半径)
            DirectX::XMFLOAT3 extents =
            {
                (b_box.local_max.x - b_box.local_min.x) * 0.5f,
                (b_box.local_max.y - b_box.local_min.y) * 0.5f,
                (b_box.local_max.z - b_box.local_min.z) * 0.5f
            };

            //ワールド変換行列
            DirectX::XMMATRIX m = DirectX::XMLoadFloat4x4(&l2w.value);

            //中心点をワールド変換
            DirectX::XMVECTOR world_center_v = DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&center),
                m
            );

            DirectX::XMFLOAT3 world_center;
            DirectX::XMStoreFloat3(&world_center, world_center_v);

            const DirectX::XMFLOAT4X4& mat = l2w.value;
            DirectX::XMFLOAT3 world_extents;
            world_extents.x =
                std::abs(mat._11) * extents.x +
                std::abs(mat._21) * extents.y +
                std::abs(mat._31) * extents.z;

            world_extents.y =
                std::abs(mat._12) * extents.x +
                std::abs(mat._22) * extents.y +
                std::abs(mat._32) * extents.z;

            world_extents.z =
                std::abs(mat._13) * extents.x +
                std::abs(mat._23) * extents.y +
                std::abs(mat._33) * extents.z;

            //ワールド空間のAABBを計算
            b_box.world_min =
            {
                world_center.x - world_extents.x,
                world_center.y - world_extents.y,
                world_center.z - world_extents.z
            };
            b_box.world_max =
            {
                world_center.x + world_extents.x,
                world_center.y + world_extents.y,
                world_center.z + world_extents.z
            };
        }
        ,1024
    );
}
