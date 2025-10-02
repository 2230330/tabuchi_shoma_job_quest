#pragma once

#include<vector>
#include<memory>
#include"i_system.h"
#include"../component/component_manager.h"
//コンポーネントを回すためのクラス
class SystemManager {
public:
    SystemManager(ComponentManager& comp_mng);

    void AddSystem(std::unique_ptr<ISystem> system) {
        systems_.emplace_back(std::move(system));
    }

    //Updateを一括で回す関数。
    //その内マルチタスクにしたいなぁ
    void UpdateAll(float elapsed_time);

private:
    std::vector<std::unique_ptr<ISystem>> systems_;
    ComponentManager& comp_mng_;
};