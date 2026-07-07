#include"../../headers/system/update_transform_system.h"
#include<DirectXMath.h>

#include"../../headers/component/component_manager.h"


UpdateTransformSystem::UpdateTransformSystem(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
}

//簡単な姿勢制御用
void UpdateTransformSystem::Update(float elapsed_time)
{
    (void)elapsed_time;

    comp_mng_.ForEach<
        ComponentLocalToWorld,
        ComponentPosition,
        ComponentRotation,
        ComponentScale,
        ComponentGltf
    >(
        [](uint32_t entity_id,
            ComponentLocalToWorld& l2w,
            ComponentPosition& pos,
            ComponentRotation& rot,
            ComponentScale& scale,
            ComponentGltf& gltf)
        {
            if (gltf.dirty)
                return;

            DirectX::XMMATRIX scale_matrix = DirectX::XMMatrixScaling(
                scale.value.x,
                scale.value.y,
                scale.value.z
            );

            DirectX::XMMATRIX rotation_matrix = DirectX::XMMatrixRotationRollPitchYaw(
                rot.value.x,
                rot.value.y,
                rot.value.z
            );

            DirectX::XMMATRIX translation_matrix = DirectX::XMMatrixTranslation(
                pos.value.x,
                pos.value.y,
                pos.value.z
            );

            DirectX::XMStoreFloat4x4(
                &l2w.value,
                scale_matrix * rotation_matrix * translation_matrix
            );
        }
    );
}