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
#include"../gltf_model.h"

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

    enum CASCADE : int
    {
        Near = 0,
        Mid,
        Far,
        CascadeCount
    };

private:
    void DirectionalShadowRendering();
    //GPU負荷計測用
    void InitializeGpuTimer(ID3D11Device* device);
    void BeginGpuFrame(int write_index);
    void UpdateGpuTimer(int read_index);
    void EndGpuFrame(int write_index);
private:
    ComponentManager& comp_mng_;
    LightManager* light_manager_ = nullptr;
    std::unique_ptr<FullscreenQuad>fullscreen_quad_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>srvs_[Target::Count];
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_directional_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_indirect_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>deferred_rendering_emissive_ps_=nullptr;

    //複数の光を合成する為、内部で弄る必要がありました。
    std::unique_ptr<RenderState>render_state_=nullptr;

    //シャドウマップ
    const float shadow_distance_ = 1500.f;
    float shadow_coverage_ = 500.f; 
    const float shadow_near_clip_plane_ = 1.f;
    const float shadow_far_clip_plane_ = 3000.f;
    const float shadowmap_width_ = 4096.f;
    const float shadowmap_height_ = 4096.f;
    const float shadowmap_fov_y_ = DirectX::XMConvertToRadians(30.f);
    DirectX::XMFLOAT4 camera_position_{ 0.f, 0.f, 0.f, 0.f };
    DirectX::XMFLOAT4 camera_front_{ 0.f, 0.f, 0.f, 0.f };
    DirectX::XMFLOAT4 camera_right_{ 0.f, 0.f, 0.f, 0.f };
    bool has_shadow_ = false;


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
        float split_aria_table[4] = {120.f,500.f,1000.f,0.0f/*dummy*/};
        
        int current_index = 0;
        int dummy[3];

    };
    CascadeShadowSceneConstants cascade_shadow_scene_constant_{}; 

    static constexpr float split_aria_table_[] = 
    {
        0.1f,
        120.f,
        500.f,
        1000.f,
    };
    std::array<std::unique_ptr<FrameBuffer>,CASCADE::CascadeCount> shadowmap_framebuffers_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadowmap_depth_stencil_view_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>shadowmap_shader_resource_view_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer>shadow_scene_constant_buffer_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer>cascade_shadow_scene_constant_buffer_=nullptr;

    //シャドウマップ用のモデルのワールド行列を保持するマップ
    std::unordered_map<GltfModel*, std::vector<DirectX::XMFLOAT4X4>> model_to_worlds_;
    std::unordered_map < GltfModel*, std::vector<const std::vector<GltfModel::Node>*>>model_to_animated_nodes_list_;

    //GPU負荷計測用
    static const int QUERY_BUFFER_COUNT = 32;
    int write_index_ = 0;
    Microsoft::WRL::ComPtr<ID3D11Query> dis_joint_query_[QUERY_BUFFER_COUNT];
    Microsoft::WRL::ComPtr<ID3D11Query>time_stamp_start_query_[QUERY_BUFFER_COUNT];
    Microsoft::WRL::ComPtr<ID3D11Query>time_stamp_end_query_[QUERY_BUFFER_COUNT];
    double shadow_gpu_time_ms_ = 0.0;

};