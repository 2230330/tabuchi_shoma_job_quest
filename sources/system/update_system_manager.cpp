#include"../../headers/system/update_system_manager.h"
#include"../../headers/system/transform_system.h"
#include"../../headers/system/camera_update_system.h"

//新しいシステムはここで登録する
UpdateSystemManager::UpdateSystemManager(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{

    AddSystem(std::make_unique<TransformSystem>(comp_mng_));
    AddSystem(std::make_unique<CameraUpdateSystem>(comp_mng_));
}

//システム追加用関数
void UpdateSystemManager::AddSystem(std::unique_ptr<IUpdateSystem> system)
{
    systems_.emplace_back(std::move(system));
}


void UpdateSystemManager::UpdateAll(float elapsed_time)
{
        for (auto& system : systems_) {
            system->Update(elapsed_time);
        }
    
}
