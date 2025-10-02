#include"../../headers/system/render_system_manager.h"
#include"../../headers/system/render_system.h"

RenderSystemManager::RenderSystemManager(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
    AddSystem(std::make_unique<RenderSystem>(comp_mng_));
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