#pragma once
#include"i_render_system.h"

#include"../deferred_g_buffer.h"
#include"../light_manager.h"
#include"../fullscreen_quad.h"
#include"../render_state.h"

class DeferredRenderSystem :public IRenderSystem
{
public: 
    DeferredRenderSystem(RenderPass render_pass);

    void Render()override;

    void SetLightManager(LightManager* light_manager) { this->light_manager_ = light_manager; }

    void SetSRV(ID3D11ShaderResourceView* srv, int num) { this->srvs_[num] = srv; }


private:

    LightManager* light_manager_ = nullptr;
    std::unique_ptr<FullscreenQuad>fullscreen_quad_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>srvs_[DeferredGBuffer::Target::Count];
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_directional_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_indirect_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_emissive_ps_=nullptr;

    //複数の光を合成する為、内部で弄る必要がありました。
    std::unique_ptr<RenderState>render_state_=nullptr;
};