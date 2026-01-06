#pragma once

#include<vector>
#include<memory>
#include"i_render_system.h"
#include"../component/component_manager.h"
#include"../world/world.h"
#include"../fullscreen_quad.h"
#include"../framebuffer.h"
//コンポーネントを回すためのクラス
class RenderSystemManager {
public:
    RenderSystemManager(ComponentManager& comp_mng);
    virtual ~RenderSystemManager() = default;

    void AddSystem(std::unique_ptr<IRenderSystem> system);

    //Updateを一括で回す関数。
    //その内マルチタスクにしたいなぁ
    void RenderAll();

private:
    //レンダリングシステム群
    void RunPass(RenderPass pass);

    std::vector<std::unique_ptr<IRenderSystem>> systems_;
    ComponentManager& comp_mng_;

    //フルスクリーンクワッド(背景用)
    std::unique_ptr<FullscreenQuad> bit_block_transfer_;
    std::unique_ptr<FrameBuffer> back_framebuffer_;
    std::unique_ptr<FrameBuffer> object_framebuffer_;

    float back_scale = 1.0f;
    int back_sample_count_ = 0;
    const int back_sample_rimit_ = 60;
};