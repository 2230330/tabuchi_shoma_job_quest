#include"../../headers/system/system_manager.h"

void SystemManager::UpdateAll(float elapsed_time)
{
        for (auto& system : systems_) {
            system->Update(elapsed_time);
        }
    
}
