#pragma once

#include<vector>
#include<memory>
#include"i_render_system.h"
#include"../component/component_manager.h"
#include"../world/world.h"
//コンポーネントを回すためのクラス
class RenderSystemManager {
public:
    RenderSystemManager(ComponentManager& comp_mng,World&world);
    virtual ~RenderSystemManager() = default;

    void AddSystem(std::unique_ptr<IRenderSystem> system);

    //Updateを一括で回す関数。
    //その内マルチタスクにしたいなぁ
    void RenderAll();

private:
    std::vector<std::unique_ptr<IRenderSystem>> systems_;
    ComponentManager& comp_mng_;
    World& world_;
};