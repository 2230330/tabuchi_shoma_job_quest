#include"../../headers/scene/scene_test.h"

#include<tchar.h>
#include<iostream>
#include<filesystem>

#include"../../headers/graphics.h"
#include"../../headers/misc.h"
#include"../../headers/constant_buffer_slot.h"
#include"../../headers/shader.h"
#include"../../external/imgui/imgui.h"

SceneTest::SceneTest(const HWND hwnd)
    :Scene(hwnd)
{

}

bool SceneTest::InitializeCore()
{

    //std::ifstream file("C:/Users/2230330/Desktop/JobQuest/part2/resources/model/fbx/nico/nico.fbx");

    //各種マネージャの設定
    {
        comp_mng = std::make_unique<ComponentManager>();
        world = std::make_unique<World>();
        comp_edit = std::make_unique<ComponentEditor>(*comp_mng, *world->GetEntityManager());
        update_sys_mng = std::make_unique<UpdateSystemManager>(*comp_mng);
        render_sys_mng = std::make_unique<RenderSystemManager>(*comp_mng);
        light_manager_ = std::make_unique<LightManager>();
        render_sys_mng->SetLightManager(light_manager_.get());

        uint32_t w = static_cast<uint32_t>(Graphics::Instance().GetScreenWidth());
        uint32_t h = static_cast<uint32_t>(Graphics::Instance().GetScreenHeight());
        post_pro_mng = std::make_unique<PostProcessManager>(
            Graphics::Instance().GetDevice(),
            w,h);
    }

    //レンダリングオブジェクト宣言
    {
        ID3D11Device* device = Graphics::Instance().GetDevice();
        //2dスプライト宣言
        {

        }
        //3dオブジェクト宣言
        {
            ResourceManager::Instance().LoadGltfModel(device, ".\\resources\\model\\gltf\\DamagedHelmet\\DamagedHelmet.gltf");
            ResourceManager::Instance().LoadGltfModel(device, ".\\resources\\model\\gltf\\blue_exagonal_tiles_with_extracted\\scene.gltf");
            ResourceManager::Instance().LoadGltfModel(device, ".\\resources\\model\\gltf\\cube.glb");

        }
        //shader
        {
            pixel_shaders_[0] = 
                ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\luminance_extraction_ps.cso");
            pixel_shaders_[1] =
                ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\blur_ps.cso");
        }
        //フレームバッファの作成
        {
            for (int i = 0; i < 2; i++)
            {
                framebuffers_[i] = std::make_unique<FrameBuffer>(
                    device,
                    Graphics::Instance().GetScreenWidth(),
                    Graphics::Instance().GetScreenHeight()
                );
            }
        }
    }

    {
        comp_edit->Load("progress.json");
        
        //for (int i = 0; i < 100; i++)
        //{
        //    uint32_t entity = world->GetEntityManager()->Add();
        //    ComponentPosition pos;
        //    pos.value = { 3.f * static_cast<float>( i % 10), 1,3.f * static_cast<float>( i / 10) };
        //    comp_mng->Add(entity, pos);
        //    ComponentRotation rot;
        //    rot.value = { 0, 0, 0 };
        //    comp_mng->Add(entity, rot);
        //    ComponentScale scale;
        //    scale.value = { 1, 1, 1 };
        //    comp_mng->Add(entity, scale);
        //    ComponentLocalToWorld l2w;
        //    comp_mng->Add(entity, l2w);
        //    ComponentAdjastPbrParamter ajust_pbr_paramter;
        //    comp_mng->Add(entity, ajust_pbr_paramter);
        //    ComponentGltf gltf;
        //    gltf.model= ResourceManager::Instance().LoadGltfModel(Graphics::Instance().GetDevice(), ".\\resources\\model\\gltf\\DamagedHelmet\\DamagedHelmet.gltf");

        //    comp_mng->Add(entity, gltf);
        //    ComponentInstanced instanced;
        //    comp_mng->Add(entity, instanced);
        //}

    }
    return true;
}

bool SceneTest::UninitializeCore()
{
    ResourceManager::Instance().Shutdown();

    return true;
}

void SceneTest::UpdateCore(float elapsed_time)
{
    light_manager_->Update(elapsed_time);
    update_sys_mng->UpdateAll(elapsed_time);
}

void SceneTest::RenderCore(float elapsed_time)
{
    HRESULT hr{ S_OK };

    FLOAT color[]{ .0f,.0f,.0f,0.f };

    ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
    RenderState* render_state = Graphics::Instance().GetRenderState();

    Graphics::Instance().ViewClear(color[0], color[1], color[2], color[3]);
    Graphics::Instance().SetRenderTargets();
    //サンプラーステート設定
    {
        Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_state;
        sampler_state = render_state->GetSamplerState(SamplerState::point_wrap);
        dc->PSSetSamplers(0, 1, sampler_state.GetAddressOf());
        sampler_state = render_state->GetSamplerState(SamplerState::point_clamp);
        dc->PSSetSamplers(1, 1, sampler_state.GetAddressOf());
        sampler_state = render_state->GetSamplerState(SamplerState::linear_wrap);
        dc->PSSetSamplers(2, 1, sampler_state.GetAddressOf());
        sampler_state = render_state->GetSamplerState(SamplerState::linear_clamp);
        dc->PSSetSamplers(3, 1, sampler_state.GetAddressOf());
        sampler_state = render_state->GetSamplerState(SamplerState::anisotropic);
        dc->PSSetSamplers(4, 1, sampler_state.GetAddressOf());
        sampler_state = render_state->GetSamplerState(SamplerState::linear_mirror);
        dc->PSSetSamplers(5, 1, sampler_state.GetAddressOf());
        sampler_state = render_state->GetSamplerState(SamplerState::shadowmap);
        dc->PSSetSamplers(6, 1, sampler_state.GetAddressOf());


    }
    //レンダーステート設定
    {
        dc->OMSetBlendState(render_state->GetBlendState(BlendState::transparency), nullptr, 0xffffffff);
        dc->OMSetDepthStencilState(render_state->GetDepthStencilState(DepthState::test_and_write), 0);
        dc->RSSetState(render_state->GetRasterizerState(RasterizerState::solid_cull_none));
    }
    //ビュープロジェクション変換行列の計算と定数バッファにセット
    {
        D3D11_VIEWPORT viewport;
        UINT num_viewports{ 1 };
        dc->RSGetViewports(&num_viewports, &viewport);

        light_manager_->SetForwardLightConstant(static_cast<UINT>(ConstantBufferSlot::kForwardLight));
        SetSceneConstant(static_cast<UINT>(ConstantBufferSlot::kPerFrame),true,light_manager_->GetDirectionLight().direction);
    }
    //レンダリングオブジェクト描画
    {
        
        render_sys_mng->RenderAll();
        
    }
    //ポストエフェクト
    {
        //post_pro_mng->PostProcess(dc, framebuffers_[0]->GetShaderResourceView(0).Get());
    }
}

void SceneTest::DrawImguiCore()
{
    float screen_width = Graphics::Instance().GetScreenWidth();
    float screen_height = Graphics::Instance().GetScreenHeight();
    float imgui_window_size_x = screen_width*0.1f;
    float imgui_window_size_h = screen_height*0.1f;
    float imgui_alpha = 0.5f;

    ImGui::SetNextWindowPos({ 0,0 }, ImGuiSetCond_Always);
    ImGui::SetNextWindowSize({ imgui_window_size_x*3.f,imgui_window_size_h*5.0f }, ImGuiSetCond_Always);
    ImGui::SetNextWindowBgAlpha(imgui_alpha);
    //ライトマネージャー
    light_manager_->DrawImgui();

    ////ポストエフェク
    //ImGui::SetNextWindowPos({ 0,imgui_window_size_h*5.f }, ImGuiSetCond_Always);
    //ImGui::SetNextWindowSize({ imgui_window_size_x * 3.f,imgui_window_size_h * 5.0f }, ImGuiSetCond_Always);
    //ImGui::SetNextWindowBgAlpha(imgui_alpha);
    //post_pro_mng->PostImgui();

    //コンポーネントマネージャ
    ImGui::SetNextWindowPos({ imgui_window_size_x*7,0 }, ImGuiSetCond_Always);
    ImGui::SetNextWindowSize({ imgui_window_size_x * 3.f,imgui_window_size_h*10.0f }, ImGuiSetCond_Always);
    ImGui::SetNextWindowBgAlpha(imgui_alpha);
    comp_edit->DrawImgui();
}
