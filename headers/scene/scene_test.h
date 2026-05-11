#ifndef PART2_SCENE_TEST_H
#define PART2_SCENE_TEST_H

#include<memory>
#include<d3d11.h>

#include"scene.h"

class ComponentManager;
class ComponentEditor;
class UpdateSystemManager;
class RenderSystemManager;
class World;
class LightManager;

class SceneTest :public Scene
{
public:
    SceneTest(const HWND hwnd);
    ~SceneTest()override = default;


private:
    bool InitializeCore()override;
    bool UninitializeCore()override;
    //更新処理
    void UpdateCore(float elapsed_time)override;
    //描画処理
    void RenderCore(float elapsed_time)override;
    //GUI描画処理
    void DrawImguiCore()override;



    std::unique_ptr<ComponentManager> comp_mng = nullptr;
    std::unique_ptr<ComponentEditor>comp_edit = nullptr;
    std::unique_ptr<UpdateSystemManager>update_sys_mng = nullptr;
    std::unique_ptr<RenderSystemManager>render_sys_mng = nullptr;
    std::unique_ptr<World>world = nullptr;
    std::unique_ptr<LightManager>light_manager_ = nullptr;
};

#endif // !PART2_SCENE_TEST_H
