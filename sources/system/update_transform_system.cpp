#include"../../headers/system/update_transform_system.h"
#include<DirectXMath.h>

TransformSystem::TransformSystem(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
}

//簡単な姿勢制御用
void TransformSystem::Update(float elapsed_time)
{
    comp_mng_.ForEach<ComponentLocalToWorld>([this,elapsed_time](uint32_t entity_id, ComponentLocalToWorld& l2w) {
        auto* pos = comp_mng_.TryGetByEntity<ComponentPosition>(entity_id);
        auto* rot = comp_mng_.TryGetByEntity<ComponentRotation>(entity_id);
        auto* scale = comp_mng_.TryGetByEntity<ComponentScale>(entity_id);

        //全て持っている場合、行列制御を行う
        if (pos && rot && scale)
        {
            DirectX::XMMATRIX scale_matrix = DirectX::XMMatrixScaling(
                scale->value.x, scale->value.y, scale->value.z
            );
            DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(
                rot->value.x, rot->value.y, rot->value.z
            );
            DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(
                pos->value.x, pos->value.y, pos->value.z
            );

            DirectX::XMStoreFloat4x4(&l2w.value, scale_matrix * rotation * translation);
        }
        }
    );
}
