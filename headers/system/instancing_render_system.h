#pragma once
#include"i_render_system.h"
#include"../component/component_manager.h"
#include"../gltf_model.h"
#include"../world/world.h"

//描画用システム
//現在は、元々用意していたGlTFモデルの描画を使っていますが、そのうちちゃんと作りたい
class InstancingRenderSystem :public IRenderSystem
{
public:
    InstancingRenderSystem(ComponentManager& comp_mng, World& world);

    void Render()override;
private:
    //幾つか描画方法を作るつもりです
    void DrawPrimitive(ID3D11DeviceContext* context, const GltfModel::Mesh::primitive& primitive, const GltfModel& model);

    ComponentManager& comp_mng_;
    World& world_;
};