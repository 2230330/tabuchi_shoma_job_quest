#pragma once
#include"i_render_system.h"

#include"../component/component_manager.h"
#include"../deferred_g_buffer.h"
#include"../light_manager.h"
#include"../fullscreen_quad.h"
#include"../render_state.h"
#include"../framebuffer.h"

class DeferredRenderSystem :public IRenderSystem
{
public: 
    DeferredRenderSystem(ComponentManager&comp_mng,RenderPass render_pass);

    void Render()override;

    void SetLightManager(LightManager* light_manager) { this->light_manager_ = light_manager; }

    void SetSRV(ID3D11ShaderResourceView* srv, int num) { this->srvs_[num] = srv; }

    void SetCameraPosition(const DirectX::XMFLOAT4& camera_position)  { this->camera_position_ = camera_position; }
private:
    void directional_shadow_rendering();

    ComponentManager& comp_mng_;
    LightManager* light_manager_ = nullptr;
    std::unique_ptr<FullscreenQuad>fullscreen_quad_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>srvs_[DeferredGBuffer::Target::Count];
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_directional_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_indirect_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_emissive_ps_=nullptr;

    //複数の光を合成する為、内部で弄る必要がありました。
    std::unique_ptr<RenderState>render_state_=nullptr;

    //シャドウマップ
    const float shadow_distance_ = 20.0f;
    float shadow_coverage_ = 20.f; // シャドウマップに収めたい範囲（影の最大距離など）
    const float shadow_near_clip_plane_ = 1.f;
    const float shadow_far_clip_plane_ = 80.f;
    const float shadow_map_size_ = 1024.0f;
    const float shadowmap_width_ = 1024.f;
    const float shadowmap_height_ = 1024.f;
    DirectX::XMFLOAT4X4 light_view_{};
    DirectX::XMFLOAT4X4 light_projection_{};
    DirectX::XMFLOAT4X4 light_view_projection_{};
    DirectX::XMFLOAT4X4 inverse_light_view_projection_{};

    //インスタンスバッファのプール
    //インスタンス化したオブジェのシャドウマップ用
    //モデルごとにバッファを用意して、必要なサイズが足りなくなったら大きいバッファに入れ替える
    struct InstanceBufferInfo {
        Microsoft::WRL::ComPtr<ID3D11Buffer>buffer;
        size_t cepasity = 0;
    };
    std::unordered_map<GltfModel*, InstanceBufferInfo>instance_buffer_pool_;

    //カメラ位置（シャドウマップの中心を決めるのに必要）
    DirectX::XMFLOAT4 camera_position_{};

    struct ShadowSceneConstants
    {
        DirectX::XMFLOAT4X4 light_view_projection;
        DirectX::XMFLOAT4X4 inverse_light_view_projection;
    };


    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadowmap_depth_stencil_view_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>shadowmap_shader_resource_view_;
    std::unique_ptr<FrameBuffer> shadowmap_framebuffer_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer>shadow_scene_constant_buffer_;
};