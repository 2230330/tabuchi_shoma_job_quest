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
        ComponentDynamic,
        ComponentBoundingBox,
        ComponentLocalToWorld
    >(
        [&](uint32_t entity_id,
            ComponentDynamic& dyn,
            ComponentBoundingBox& b_box,
            ComponentLocalToWorld& l2w )
        {
            

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

            // 行ベクトル規約なので、各行の長さが実効スケール
            const float matrix_scale_x = std::sqrt(
                mat._11 * mat._11 +
                mat._12 * mat._12 +
                mat._13 * mat._13
            );

            const float matrix_scale_y = std::sqrt(
                mat._21 * mat._21 +
                mat._22 * mat._22 +
                mat._23 * mat._23
            );

            const float matrix_scale_z = std::sqrt(
                mat._31 * mat._31 +
                mat._32 * mat._32 +
                mat._33 * mat._33
            );

            DirectX::XMFLOAT3 world_extents =
            {
                std::abs(extents.x * mat._11) +
                std::abs(extents.y * mat._21) +
                std::abs(extents.z * mat._31),

                std::abs(extents.x * mat._12) +
                std::abs(extents.y * mat._22) +
                std::abs(extents.z * mat._32),

                std::abs(extents.x * mat._13) +
                std::abs(extents.y * mat._23) +
                std::abs(extents.z * mat._33)
            };
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
