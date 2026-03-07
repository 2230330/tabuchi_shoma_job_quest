#pragma once
#include"i_render_system.h"

#include"../deferred_g_buffer.h"
#include"../light_manager.h"
#include"../fullscreen_quad.h"
#include"../render_state.h"
#include"../framebuffer.h"

class DeferredRenderSystem :public IRenderSystem
{
public: 
    DeferredRenderSystem(RenderPass render_pass);

    void Render()override;

    void SetLightManager(LightManager* light_manager) { this->light_manager_ = light_manager; }

    void SetSRV(ID3D11ShaderResourceView* srv, int num) { this->srvs_[num] = srv; }


private:
    void directional_shadow_rendering();

    LightManager* light_manager_ = nullptr;
    std::unique_ptr<FullscreenQuad>fullscreen_quad_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>srvs_[DeferredGBuffer::Target::Count];
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_directional_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_indirect_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_emissive_ps_=nullptr;

    //複数の光を合成する為、内部で弄る必要がありました。
    std::unique_ptr<RenderState>render_state_=nullptr;

    //シャドウマップのサイズ
    const float shadowmap_width_ = 1024.f;
    const float shadowmap_height_ = 1024.f;

    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadowmap_depth_stencil_view_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>shadowmap_shader_resource_view_;
    std::unique_ptr<FrameBuffer> shadowmap_framebuffer_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer>shadow_scene_constant_buffer_;
};