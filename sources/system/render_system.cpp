#include"../../headers/system/render_system.h"
#include"../../headers/graphics.h"

RenderSystem::RenderSystem(ComponentManager& comp_mng,World&world)
    :comp_mng_(comp_mng),world_(world)
{
}

void RenderSystem::Render()
{
    comp_mng_.ForEach<ComponentGltf>([this](uint32_t entity_id, ComponentGltf& gltf)
        {
            auto* l2w = comp_mng_.TryGetByEntity < ComponentLocalToWorld>(entity_id);
            auto* ins = comp_mng_.TryGetByEntity<ComponentInstanced>(entity_id);


            //—v‹‚µ‚½‚à‚Ì‚ª‚ ‚Á‚½‚ç
            if (l2w&&!ins)
            {
                gltf.model->Render(Graphics::Instance().GetDeviceContext(), l2w->value);
            }
        });
}
