#include"../../headers/system/render_gltf_system.h"
#include"../../headers/graphics.h"
#include"../../headers/component/component_manager.h"
#include"../../headers/component/component_bound_box.h"
#include"../../headers/system/render_frustum_helper.h"


GltfRenderSystem::GltfRenderSystem(ComponentManager& comp_mng,RenderPass render_pass)
    :comp_mng_(comp_mng)
    , IRenderSystem(render_pass)
{
}

void GltfRenderSystem::Render()
{
    comp_mng_.ForEach<
        ComponentGltf,
        ComponentLocalToWorld,
        ComponentBoundingBox
    >([this](
        uint32_t entity_id,
        ComponentGltf& gltf,
        ComponentLocalToWorld& l2w,
        ComponentBoundingBox& b_box
        )
        {
            if (!comp_mng_.TryGetByEntity<ComponentSkyAtmosphere>(entity_id) &&
                !comp_mng_.TryGetByEntity<ComponentVolumetricCloud>(entity_id))
            {
                auto* ins = comp_mng_.TryGetByEntity<ComponentInstanced>(entity_id);
                
                //フラスタムカリングのためにメインカメラを取得
                ComponentCamera* main_camera = nullptr;
                comp_mng_.ForEach<ComponentCamera>([&](uint32_t entity_id, ComponentCamera& camera)
                    {
                        if (camera.main_camera_flag_)
                        {
                            main_camera = &camera;
                        }
                    });
                //フラスタムカリング
                if (main_camera)
                {
                    std::array<DirectX::XMFLOAT4, 6>frustum_planes{};
                    FrustumHelper::CreateFrustumPlanesFromMatrix(main_camera->view_projection_transform, frustum_planes);
                    if (FrustumHelper::IsValidWorldBoundingBox(b_box))
                    {
                        if (!FrustumHelper::IsAABBVisibleFromFrustumPlanes(b_box, frustum_planes))
                        {
                            return;
                        }
                    }
                }
                
                
                //インスタンス化していない場合は通常描画
                if (!ins)
                {
                    auto* adjast = comp_mng_.TryGetByEntity<ComponentAdjastPbrParamter>(entity_id);
                    if (adjast)
                    {
                        gltf.model->SetAdjastParam(
                            adjast->adjust_metalness,
                            adjast->adjust_roughness);
                    }

                    gltf.model->Render(Graphics::Instance().GetDeviceContext(), l2w.value,gltf.animated_nodes);
                }
            }
        });
}
