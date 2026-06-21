#pragma once
#include"i_render_system.h"
#include<d3d11.h>
#include<wrl.h>
#include<unordered_map>

//前方宣言
class ComponentManager;
class GltfModel;

//描画用システム
//現在は、元々用意していたGlTFモデルの描画を使っていますが、そのうちちゃんと作りたい
class InstancingRenderSystem :public IRenderSystem
{

public:
    InstancingRenderSystem(ComponentManager& comp_mng,RenderPass render_pass);

    void Render()override;
private:
    //幾つか描画方法を作るつもりです

    struct InstanceBufferInfo {
        Microsoft::WRL::ComPtr<ID3D11Buffer>buffer;
        size_t cepasity = 0;
    };

    std::unordered_map<GltfModel*, InstanceBufferInfo>instance_buffer_pool_;
    ComponentManager& comp_mng_;
};