#pragma once

#include<vector>
#include<memory>
#include<wrl.h>
#include"i_render_system.h"
#include"../../headers/system/sky_render_system.h"
#include"../../headers/system/cloud_render_system.h"
#include"../../headers/system/deferred_render_system.h"
#include"ibl_manager.h"
#include"../component/component_manager.h"
#include"../world/world.h"
#include"../fullscreen_quad.h"
#include"../framebuffer.h"
#include"../deferred_g_buffer.h"
#include"../light_manager.h"
#include"camera_set_constants.h"
//コンポーネントを回すためのクラス
class RenderSystemManager {
public:
    RenderSystemManager(ComponentManager& comp_mng);
    virtual ~RenderSystemManager() = default;

    void AddSystem(std::unique_ptr<IRenderSystem> system);

    //Updateを一括で回す関数。
    //その内マルチタスクにしたいなぁ
    void RenderAll();

    void SetLightManager(LightManager* light_manager) { this->light_manager_ = light_manager; }
private:
    //レンダリングシステム群
    void RunPass(RenderPass pass);

    std::vector<std::unique_ptr<IRenderSystem>> systems_;
    //背景用描画システムは別枠で管理する
    std::unique_ptr<SkyRenderSystem>sky_render_system_;
    std::unique_ptr<CloudRenderSystem>cloud_render_system_;
    std::unique_ptr<DeferredRenderSystem>deferred_render_system_;
    ComponentManager& comp_mng_;

    //フルスクリーンクワッド(背景用)
    std::unique_ptr<FullscreenQuad> bit_block_transfer_;
    std::unique_ptr<FrameBuffer> back_framebuffer_;
    std::unique_ptr<FrameBuffer> object_framebuffer_;
    std::unique_ptr<FrameBuffer> sky_framebuffer_;
    std::unique_ptr<FrameBuffer> final_framebuffer_;
    std::unique_ptr<DeferredGBuffer> deferred_framebuffer_;

    //FXAA
    Microsoft::WRL::ComPtr<ID3D11PixelShader>fxaa_ps_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>history_color_srv_=nullptr;

    //IBLマネージャ
    std::unique_ptr<IBLManager> ibl_manager_ ;

    std::unique_ptr<CameraSetConstants> camera_set_constants_;

    //ライト情報 
    LightManager* light_manager_ = nullptr;
    
    //スカイキューブの分割
    int ibl_steps_per_frame_ = 1;

    const float back_scale_ = 2.0f;
    const float obj_scale = 1.0f;
    //背景のサンプリング間隔
    //マイフレーム呼び出す必要はないと感じました
    int back_sample_count_ = 0;
    const int back_sample_rimit_ = 2;

    Microsoft::WRL::ComPtr<ID3D11PixelShader> celestial_light_ps_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> light_shafts_ps_;
};