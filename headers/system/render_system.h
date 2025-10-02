#pragma once
#include"i_render_system.h"
#include"../component/component_manager.h"

//描画用システム
//現在は、元々用意していたGlTFモデルの描画を使っていますが、そのうちちゃんと作りたい
class RenderSystem :public IRenderSystem
{
public:
    RenderSystem(ComponentManager& comp_mng);

    void Render()override;
private:
    ComponentManager& comp_mng_;

};