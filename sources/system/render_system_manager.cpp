#include"../../headers/system/render_system_manager.h"

#include"../../headers/graphics.h"
#include"../../headers/system/render_system.h"
#include"../../headers/system/instancing_render_system.h"
#include"../../headers/system/sprite_render_system.h"
#include"../../headers/system/sky_render_system.h"
#include"../../headers/system/cloud_render_system.h"


RenderSystemManager::RenderSystemManager(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
    AddSystem(std::make_unique<SkyRenderSystem>(comp_mng_));
    AddSystem(std::make_unique<CloudRenderSystem>(comp_mng_));
    AddSystem(std::make_unique<RenderSystem>(comp_mng_));
    AddSystem(std::make_unique<InstancingRenderSystem>(comp_mng_));
    AddSystem(std::make_unique<SpriteRenderSystem>(comp_mng_));
}

void RenderSystemManager::AddSystem(std::unique_ptr<IRenderSystem> system)
{
    systems_.emplace_back(std::move(system));
}

void RenderSystemManager::RenderAll()
{
    for (auto& system : systems_) {
        system->Render();
    }
}