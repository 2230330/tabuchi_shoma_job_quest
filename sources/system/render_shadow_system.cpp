#include"../../headers/system/render_shadow_system.h"
#include"../../headers/graphics.h"
#include"../../headers/misc.h"
#include"../../headers/scene/scene.h"

RenderShadowSystem::RenderShadowSystem(ComponentManager& comp_mng, RenderPass render_pass)
    : IRenderSystem(render_pass)
    , comp_mng_(std::make_unique<ComponentManager>(comp_mng))
{


}

void RenderShadowSystem::Render()
{
}
