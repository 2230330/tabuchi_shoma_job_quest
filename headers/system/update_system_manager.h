#pragma once

#include<vector>
#include<memory>
#include"i_update_system.h"
#include"../component/component_manager.h"
//コンポーネントを回すためのクラス
//更新システムは、カメラの更新や物理演算など、ゲームのロジックを担当する複数のシステムで構成されます。
//このクラスは、これらのシステムを管理し、必要に応じて更新を行います。
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