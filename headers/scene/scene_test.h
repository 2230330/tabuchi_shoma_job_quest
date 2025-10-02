#ifndef PART2_SCENE_TEST_H
#define PART2_SCENE_TEST_H

#include<memory>
#include<d3d11.h>

#include"scene.h"
#include"../scene_constants.h"
#include"../camera.h"
#include"../light.h"
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

class SceneTest :public Scene
{
public:
    SceneTest();
    ~SceneTest()override = default;

    //çXêVèàóù
    void Update(float elapsed_time)override;
    //ï`âÊèàóù
    void Render(float elapsed_time)override;
    //GUIï`âÊèàóù
    void DrawGui()override;

private:
    Camera                               camera_;
    SceneLightManager                    light_manager_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shaders_[8];

    std::unique_ptr<SpriteBatch>         sprite_batches_[8];
    std::unique_ptr<GeometricPrimitive>  geometric_primitives_[8];
    std::unique_ptr<StaticMesh>          static_meshes_[8];
    std::unique_ptr<FrameBuffer>         framebuffers_[2];
    std::unique_ptr<fullscreen_quad>     bit_block_transfer_;

    struct Model {
        DirectX::XMFLOAT3 position{ 0,0,0 };
        DirectX::XMFLOAT3 rotation{ 0,0,0 };
        DirectX::XMFLOAT3 scale{ 0.5f,0.5f,0.5f };
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4 color{ 1,1,1,1 };

        std::shared_ptr<SkinnedMesh> skinned_mesh = nullptr;
    };
    Model model;
    struct GltfMesh {
        //DirectX::XMFLOAT3 position{ 0,0,0 };
        //DirectX::XMFLOAT3 rotation{ 0,0,0 };
        //DirectX::XMFLOAT3 scale{ 0.05f,0.05f,0.05f };
        //DirectX::XMFLOAT4X4 world;
        //DirectX::XMFLOAT4 color{ 1,1,1,1 };


        int entity;
        std::shared_ptr<GltfModel> gltf_mesh = nullptr;
    };
    GltfMesh gltf_model;

    std::unique_ptr<ComponentManager> comp_mng;
    std::unique_ptr<ResourceManager>rsc_mng;
    std::unique_ptr<EntityManager>enti_mng;
    std::unique_ptr<ComponentEditor>comp_edit;
    std::unique_ptr<UpdateSystemManager>update_sys_mng;
    std::unique_ptr<RenderSystemManager>render_sys_mng;
};

#endif // !PART2_SCENE_TEST_H
