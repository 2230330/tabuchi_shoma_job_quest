#pragma once
#include<d3d11.h>
#include<wrl.h>
#include<DirectXMath.h>
#include<memory>
#include<array>
#include<unordered_map>

#include"i_render_system.h"
#include"../deferred_g_buffer.h"
#include"../render_state.h"


class ComponentManager;
class GltfModel;
class FrameBuffer;
class RenderState;
class LightManager;
class FullscreenQuad;

//ディファードレンダリングのシステム
//オブジェクト描画時にGバッファに必要な情報を描画しておいて、
//この描画システムでライティングパスで光の情報と合成して最終的な色を出力します。
//シャドウマップの生成もこのシステムで行います。
class RenderDeferredSystem :public IRenderSystem
{
public: 
    RenderDeferredSystem(ComponentManager&comp_mng,RenderPass render_pass);
    ~RenderDeferredSystem();

    void Render()override;

    void SetLightManager(LightManager* light_manager);

    void SetSRV(ID3D11ShaderResourceView* srv, int num);

    void SetSSRSRV(ID3D11ShaderResourceView* ssr_srv);
    
    enum CASCADE : int
    {
        Near = 0,
        Mid,
        Far,
        CascadeCount
    };

private:
    void directional_shadow_rendering();
    ComponentManager& comp_mng_;
    LightManager* light_manager_ = nullptr;
    std::unique_ptr<FullscreenQuad>fullscreen_quad_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>srvs_[Target::Count];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>ssr_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_directional_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_indirect_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_emissive_ps_=nullptr;

    //複数の光を合成する為、内部で弄る必要がありました。
    std::unique_ptr<RenderState>render_state_=nullptr;

    //シャドウマップ
    const float shadow_distance_ = 250.0f;
    float shadow_coverage_ = 30.f; // シャドウマップに収めたい範囲（影の最大距離など）
    const float shadow_near_clip_plane_ = 1.f;
    const float shadow_far_clip_plane_ = 1000.f;
    const float shadowmap_width_ = 4096.f;
    const float shadowmap_height_ = 4096.f;
    const float shadowmap_fov_y_ = DirectX::XMConvertToRadians(90.f);
    DirectX::XMFLOAT4 camera_position_{ 0.f, 0.f, 0.f, 0.f };
    DirectX::XMFLOAT4 camera_front_{ 0.f, 0.f, 0.f, 0.f };
    DirectX::XMFLOAT4 camera_right_{ 0.f, 0.f, 0.f, 0.f };


    //インスタンスバッファのプール
    //インスタンス化したオブジェのシャドウマップ用
    //モデルごとにバッファを用意して、必要なサイズが足りなくなったら大きいバッファに入れ替える
    struct InstanceBufferInfo 
    {
        Microsoft::WRL::ComPtr<ID3D11Buffer>buffer;
        size_t cepasity = 0;
    };
    std::unordered_map<GltfModel*, InstanceBufferInfo>instance_buffer_pool_;

    struct CascadeShadowSceneConstants
    {
        DirectX::XMFLOAT4X4 light_view_projection[4];
        DirectX::XMFLOAT4X4 inverse_light_view_projection;
        int current_index = 0;
        int dummy[3];
    };
    CascadeShadowSceneConstants cascade_shadow_scene_constant_{}; 

    static constexpr float split_aria_table_[] = 
    {
        0.1f,
        25.f,
        100.f,
        250.f,
        500.f,
    };
    std::array<std::unique_ptr<FrameBuffer>,CASCADE::CascadeCount> shadowmap_framebuffers_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadowmap_depth_stencil_view_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>shadowmap_shader_resource_view_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer>shadow_scene_constant_buffer_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer>cascade_shadow_scene_constant_buffer_=nullptr;
};