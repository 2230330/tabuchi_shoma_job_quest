#pragma once
#include"i_render_system.h"
#include<d3d11.h>
#include<wrl.h>
#include<memory>

#include"../component/component_manager.h"

//シャドウマップを生成するためのシステム
class RenderShadowSystem : public IRenderSystem
{
public:
    RenderShadowSystem(ComponentManager& comp_mng, RenderPass render_pass);

    void Render()override;

    ID3D11ShaderResourceView* GetShadowMap() { return shadowmap_shader_resource_view_.Get(); }

private:
    std::unique_ptr<ComponentManager>comp_mng_;

    //シャドウマップのサイズ
    const float shadowmap_width_ = 1024.f;
    const float shadowmap_height_ = 1024.f;

    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadowmap_depth_stencil_view_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>shadowmap_shader_resource_view_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>shadow_scene_constant_buffer_;

};