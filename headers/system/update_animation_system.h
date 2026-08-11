#pragma once
#include"i_update_system.h"

//前方宣言
class ComponentManager;

//アニメーション制御用システム。
//具体的な動きを追加したいときは、別のものを用意する予定
class UpdateAnimationSystem :public IUpdateSystem
{
public:
    UpdateAnimationSystem(ComponentManager& comp_mng);

    void Update(float elapsed_time)override;

private:
    ComponentManager& comp_mng_;
};