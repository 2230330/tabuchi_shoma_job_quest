#pragma once
#include"i_update_system.h"

//前方宣言
class ComponentManager;

//境界ボックス更新用システム。
class UpdateBoundingBoxSystem :public IUpdateSystem
{
public:
    UpdateBoundingBoxSystem(ComponentManager& comp_mng);

    void Update(float elapsed_time)override;

private:
    ComponentManager& comp_mng_;
};