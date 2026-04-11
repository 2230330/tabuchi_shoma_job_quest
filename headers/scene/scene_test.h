#ifndef PART2_SCENE_TEST_H
#define PART2_SCENE_TEST_H

#include<memory>
#include<d3d11.h>

#include"scene.h"
#include"../camera.h"
#include"../light_manager.h"
#include"../sprite_batch.h"
#include"../geometric_primitive.h"
#include"../static_mesh.h"
#include"../skinned_mesh.h"
#include"../framebuffer.h"
#include"../fullscreen_quad.h"
#include"../gltf_model.h"
#include"../component/component_manager.h"
#include"../resource_manager.h"
#include"../entity/entity_manager.h"
#include"../component/component_editor.h"
#include"../system/update_system_manager.h"
#include"../system/render_system_manager.h"
#include"../world/world.h"
#include"../post_process/post_process_manager.h"

class SceneTest :public Scene
{
public:
    SceneTest(const HWND hwnd);
    ~SceneTest()override = default;


private:
    bool InitializeCore()override;
    bool UninitializeCore()override;
    //çXêVèàóù
    void UpdateCore(float elapsed_time)override;
    //ï`âÊèàóù
    void RenderCore(float elapsed_time)override;
    //GUIï`âÊèàóù
    void DrawImguiCore()override;

    Camera                               camera_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shaders_[8];

    std::unique_ptr<GeometricPrimitive>  geometric_primitives_[8];
    std::unique_ptr<StaticMesh>          static_meshes_[8];
    std::unique_ptr<FrameBuffer>         framebuffers_[2];
    std::unique_ptr<FullscreenQuad>     bit_block_transfer_;


    std::unique_ptr<ComponentManager> comp_mng;
    std::unique_ptr<ComponentEditor>comp_edit;
    std::unique_ptr<UpdateSystemManager>update_sys_mng;
    std::unique_ptr<RenderSystemManager>render_sys_mng;
    std::unique_ptr<World>world;
    std::unique_ptr<LightManager>light_manager_;
};

#endif // !PART2_SCENE_TEST_H
