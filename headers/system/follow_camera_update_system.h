#pragma once
#include"i_update_system.h"
#include"../component/component_manager.h"

class FollowCameraUpdateSystem :public IUpdateSystem
{
public:
    FollowCameraUpdateSystem(ComponentManager& comp_mng);

    void Update(float elapsed_time)override;

private:
    ComponentManager& comp_mng_;
};