#pragma once
#include"i_render_system.h"
#include"../component/component_manager.h"
#include"../gltf_model.h"
#include"../world/world.h"

//描画用システム
//現在はGlTFモデルの描画を使っていますが、そのうちちゃんと作りたい
class GltfRenderSystem :public IRenderSystem
{
public:
    GltfRenderSystem(ComponentManager& comp_mng,RenderPass render_pass);

    void SetCubeMap(ID3D11ShaderResourceView* cube_map_srv){
        cube_map_srv_ = cube_map_srv;
    }
    
    void Render()override;
private:
    //幾つか描画方法を作るつもりです
    //void DrawPrimitive(ID3D11DeviceContext* context, const GltfModel::Mesh::primitive& primitive, const GltfModel& model);

    ComponentManager& comp_mng_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cube_map_srv_ = nullptr;
};