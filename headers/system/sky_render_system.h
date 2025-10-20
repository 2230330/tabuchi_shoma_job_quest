#pragma once
#include"i_render_system.h"
#include"../component/component_manager.h"

class SkyRenderSystem:public IRenderSystem
{
public:
    SkyRenderSystem(ComponentManager& comp_mng);

    void Render()override;

private:
    ComponentManager& comp_mng_;
};