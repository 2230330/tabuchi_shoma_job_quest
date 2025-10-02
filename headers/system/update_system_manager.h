#pragma once

#include<vector>
#include<memory>
#include"i_update_system.h"
#include"../component/component_manager.h"
//コンポーネントを回すためのクラス
class UpdateSystemManager {
public:
    UpdateSystemManager(ComponentManager& comp_mng);
    virtual ~UpdateSystemManager() = default;

    void AddSystem(std::unique_ptr<IUpdateSystem> system);

    //Updateを一括で回す関数。
    //その内マルチタスクにしたいなぁ
    void UpdateAll(float elapsed_time);

private:
    std::vector<std::unique_ptr<IUpdateSystem>> systems_;
    ComponentManager& comp_mng_;
};