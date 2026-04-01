#pragma once
#include"i_update_system.h"
#include"../component/component_manager.h"

//姿勢制御用システム。
//具体的な動きを追加したいときは、別のものを用意する予定
class TransformSystem :public IUpdateSystem
{
public:
    TransformSystem(ComponentManager& comp_mng);

    void Update(float elapsed_time)override;

private:
    ComponentManager& comp_mng_;
};