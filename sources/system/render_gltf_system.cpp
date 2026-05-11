#include"../../headers/system/render_gltf_system.h"
#include"../../headers/graphics.h"
#include"../../headers/component/component_manager.h"


GltfRenderSystem::GltfRenderSystem(ComponentManager& comp_mng,RenderPass render_pass)
    :comp_mng_(comp_mng)
    , IRenderSystem(render_pass)
{
}

void GltfRenderSystem::Render()
{
    comp_mng_.ForEach<ComponentGltf>([this](uint32_t entity_id, ComponentGltf& gltf)
        {
            if (!comp_mng_.Has<ComponentSkyAtmosphere>(entity_id) &&
                !comp_mng_.Has<ComponentVolumetricCloud>(entity_id))
            {
                auto* l2w = comp_mng_.TryGetByEntity < ComponentLocalToWorld>(entity_id);
                auto* ins = comp_mng_.TryGetByEntity<ComponentInstanced>(entity_id);
                
                //要求したものがあったら
                if (l2w && !ins)
                {
                    auto* adjast = comp_mng_.TryGetByEntity<ComponentAdjastPbrParamter>(entity_id);
                    if (adjast)
                    {
                        gltf.model->SetAdjastParam(
                            adjast->adjust_metalness,
                            adjast->adjust_roughness);
                    }

                    gltf.model->Render(Graphics::Instance().GetDeviceContext(), l2w->value);
                }
            }
        });
}
