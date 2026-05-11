#pragma once

#include<d3d11.h>
#include<wrl.h>
#include<memory>
#include<vector>

#include"../system/i_render_system.h"

//前方宣言
class ComponentManager;
class FullscreenQuad;
class FrameBuffer;
class DeferredGBuffer;
class LightManager;

class IRenderSystem;
class RenderSkySystem;
class RenderCloudSystem;
class RenderDeferredSystem;
class RenderScreenSpaceReflectionSystem;
class IBLManager;
class CameraSetConstants;
class PostProcessManager;

//描画システムを管理するクラス
//描画システムは、背景、オブジェクト、UIなどの描画を担当する複数のシステムで構成されます。
//このクラスは、これらのシステムを管理し、必要に応じて更新や描画を行います。
//将来的には、描画システムの追加や削除、描画順序の管理などもこのクラスで行う予定です。
//現在は、背景用の描画システムとオブジェクト用の描画システムを管理しています。
//背景用の描画システムは、スカイレンダリングとクラウドレンダリングの2つで構成されており、
//オブジェクト用の描画システムは、デファードレンダリングの1つで構成されています。
class RenderSystemManager {
public:
    RenderSystemManager(ComponentManager& comp_mng);
    ~RenderSystemManager();

    //システムを追加
    void AddSystem(std::unique_ptr<IRenderSystem> system);

    //Updateを一括で回す関数。
    //その内マルチタスクにしたいなぁ
    void RenderAll();

    void SetLightManager(LightManager* light_manager);
private:
    //レンダリングシステム群
    void RunPass(RenderPass pass);

    std::vector<std::unique_ptr<IRenderSystem>> systems_;
    //背景用描画システムは別枠で管理する
    std::unique_ptr<RenderSkySystem>sky_render_system_=nullptr;
    std::unique_ptr<RenderCloudSystem>cloud_render_system_=nullptr;
    std::unique_ptr<RenderDeferredSystem>deferred_render_system_=nullptr;
    std::unique_ptr<RenderScreenSpaceReflectionSystem>ssr_render_system_=nullptr;
    ComponentManager& comp_mng_;

    //フルスクリーンクワッド(背景用)
    std::unique_ptr<FullscreenQuad> bit_block_transfer_=nullptr;
    std::unique_ptr<FrameBuffer> back_framebuffer_=nullptr;
    std::unique_ptr<FrameBuffer> object_framebuffer_=nullptr;
    std::unique_ptr<FrameBuffer> sky_framebuffer_=nullptr;
    std::unique_ptr<FrameBuffer> final_framebuffer_=nullptr;
    std::unique_ptr<DeferredGBuffer> deferred_framebuffer_=nullptr;


    //IBLマネージャ
    std::unique_ptr<IBLManager> ibl_manager_ =nullptr;

    std::unique_ptr<CameraSetConstants> camera_set_constants_=nullptr;

    //ライト情報 
    LightManager* light_manager_ = nullptr;
    
    //スカイキューブの分割
    int ibl_steps_per_frame_ = 1;

    const float back_scale_ = 4.0f;
    const float obj_scale = 1.0f;
    //背景のサンプリング間隔
    //マイフレーム呼び出す必要はないと感じました
    int back_sample_count_ = 0;
    const int back_sample_rimit_ = 2;

    Microsoft::WRL::ComPtr<ID3D11PixelShader> celestial_light_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> light_shafts_ps_=nullptr;

    //ポストエフェクトを管理しているマネージャ  
    std::unique_ptr<PostProcessManager> post_process_manager_=nullptr;
};