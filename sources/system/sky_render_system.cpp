#include"../../headers/system/sky_render_system.h"

SkyRenderSystem::SkyRenderSystem(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
}

void SkyRenderSystem::Render()
{
    comp_mng_.ForEach<ComponentSkyAtmosphere>([&](uint32_t entity_id, ComponentSkyAtmosphere&) {

        });
}
