#include"../../headers/scene/scene_test.h"

#include<tchar.h>
#include<iostream>
#include<filesystem>

#include"../../headers/graphics.h"
#include"../../headers/misc.h"
#include"../../external/imgui/imgui.h"
#include"../../headers/shader.h"

SceneTest::SceneTest()
{
    //std::ifstream file("C:/Users/2230330/Desktop/JobQuest/part2/resources/model/fbx/nico/nico.fbx");

    //各種マネージャの設定
    {
        comp_mng = std::make_unique<ComponentManager>();
        rsc_mng = std::make_unique<ResourceManager>();
        enti_mng = std::make_unique<EntityManager>();
        comp_edit = std::make_unique<ComponentEditor>(*comp_mng, *enti_mng);
        update_sys_mng = std::make_unique<UpdateSystemManager>(*comp_mng);
        render_sys_mng = std::make_unique<RenderSystemManager>(*comp_mng);
    }

    //レンダリングオブジェクト宣言
    {
        ID3D11Device* device = Graphics::Instance().GetDevice();
        //2dスプライト宣言
        {
            sprite_batches_[0]
                = std::make_unique<SpriteBatch>(
                    device,
                    L".\\resources\\sprite\\mamizo.png",
                    2048);
        }
        //3dオブジェクト宣言
        {
            model.skinned_mesh = std::make_shared<SkinnedMesh>(
                device,
                L".\\resources\\model\\fbx\\nico\\nico.fbx"
                , true

            );

            //gltf_model.gltf_mesh = rsc_mng->LoadGltfModel(
            //    device,
            //    ".\\resources\\model\\gltf\\knife.glb"
            //    );
        }
        //shader
        {

            //shader_from_cso::CreatePsFromCso(
            //    device,
            //    ".\\resources\\shader\\luminance_extraction_ps.cso",
            //    pixel_shaders_[0].GetAddressOf());
            //shader_from_cso::CreatePsFromCso(
            //    device,
            //    ".\\resources\\shader\\blur_ps.cso",
            //    pixel_shaders_[1].GetAddressOf());
            pixel_shaders_[0] = 
                rsc_mng->LoadPixelShader(device, L".\\resources\\shader\\luminance_extraction_ps.cso");
            pixel_shaders_[1] =
                rsc_mng->LoadPixelShader(device, L".\\resources\\shader\\blur_ps.cso");
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
        //
        {
            bit_block_transfer_ = std::make_unique<fullscreen_quad>(device);
        }
    }
    //定数バッファ用意
    {
        ID3D11Device* device = Graphics::Instance().GetDevice();

        HRESULT hr{ S_OK };
        D3D11_BUFFER_DESC buffer_desc{};
        buffer_desc.ByteWidth = sizeof(SceneConstants);
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = 0;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;
        hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer_.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }
    //カメラの設定
    {
        camera_.SetLookAt(
            { 0.0f,0.0f,-10.f },
            { 0.0f,0.0f,0.0f },
            { 0.0f,1.0f,0.0f }
        );
    }
    //ライトの設定
    {
        DirectionLight light;
        light.direction = { 0,0,1,0 };
        light.color = { 1,1,1,1 };
        light_manager_.SetDirectionLight(light);
    }
    //コンポーネントの設定(仮)
    {
        ID3D11Device* device = Graphics::Instance().GetDevice();
        gltf_model.entity = enti_mng->Add();

        ComponentGltf gltf;
        gltf.model = rsc_mng->LoadGltfModel(
            device,
            ".\\resources\\model\\gltf\\knife.glb"
        );
        comp_mng->Add<ComponentGltf>(gltf_model.entity,gltf);
        ComponentPosition pos;
        pos.value={0.f, 0.f, 0.f};
        comp_mng->Add<ComponentPosition>(gltf_model.entity,pos);
        ComponentRotation ros;
        ros.value = { 0.f,0.f,0.f };
        comp_mng->Add<ComponentRotation>(gltf_model.entity,ros);
        ComponentScale scale;
        scale.value = { 1.f,1.f,1.f };
        comp_mng->Add<ComponentScale>(gltf_model.entity,scale);
        ComponentLocalToWorld world;
        DirectX::XMStoreFloat4x4(&world.value, DirectX::XMMatrixIdentity());
        comp_mng->Add(gltf_model.entity,world);
        ComponentColor col;
        col.value = { 1,1,1,1 };
        comp_mng->Add(gltf_model.entity,col);
    }
}

void SceneTest::Update(float elapsed_time)
{
    update_sys_mng->UpdateAll(elapsed_time);
}

void SceneTest::Render(float elapsed_time)
{
    HRESULT hr{ S_OK };

    FLOAT color[]{ .5f,.5f,.5f,1.f };

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

        float aspect_ratio{ viewport.Width / viewport.Height };
        camera_.SetPerspectiveFov(DirectX::XMConvertToRadians(30), aspect_ratio, 0.1f, 100.f);
        DirectX::XMFLOAT4X4 projection = camera_.GetProjection();
        DirectX::XMMATRIX P = { XMLoadFloat4x4(&projection) };
        DirectX::XMFLOAT4X4 view = camera_.GetView();
        DirectX::XMMATRIX V = { XMLoadFloat4x4(&view) };

        SceneConstants data{};
        DirectX::XMStoreFloat4x4(&data.view_projection, V * P);
        data.light_direction = light_manager_.GetDirectionLight().direction;
        data.camera_position = { camera_.GetEye().x,camera_.GetEye().y,camera_.GetEye().z,1.0f };
        dc->UpdateSubresource(constant_buffer_.Get(), 0, 0, &data, 0, 0);
        dc->VSSetConstantBuffers(1, 1, constant_buffer_.GetAddressOf());
        dc->PSSetConstantBuffers(1, 1, constant_buffer_.GetAddressOf());
    }
    //レンダリングオブジェクト描画
    {
        framebuffers_[0]->Clear(dc);
        framebuffers_[0]->Activate(dc);
        //2Dスプライト描画
        {
            dc->OMSetDepthStencilState(render_state->GetDepthStencilState(DepthState::test_only), 0);

            sprite_batches_[0].get()->begin(dc);
            //sprite_batches_[0].get()->Render(dc, 0, 0, 1280, 720);
            sprite_batches_[0].get()->end(dc);

            dc->OMSetDepthStencilState(render_state->GetDepthStencilState(DepthState::test_and_write), 0);

        }
        //3dオブジェクト描画
        {

            //アニメーションキーフレームの更新
            //int clip_index = 0;
            //int frame_index = 0;
            //static float animation_tick = 0;
            //auto& animation = model.skinned_mesh->GetAnimationClip(clip_index);
            //{
            //    frame_index = static_cast<int>(animation_tick * animation.sampling_rate);
            //    if (frame_index > animation.sequence.size() - 1)
            //    {
            //        frame_index = 0;
            //        animation_tick = 0;
            //    }
            //    else
            //    {
            //        animation_tick += elapsed_time;
            //    }
            //}
            //Animation::KeyFrame& keyframe = animation.sequence.at(frame_index);
            //this->model.skinned_mesh->Render(dc, model.world, model.color,&keyframe);

            //static std::vector<GltfModel::Node> animated_nodes{ gltf_model.gltf_mesh->GetNodes()};
            //static float time{ 0 };
            //if (this->gltf_model.gltf_mesh->GetAnimations().size() > 0)
            //{
            //    this->gltf_model.gltf_mesh->Animate(0, time += elapsed_time, animated_nodes);
            //    if (this->gltf_model.gltf_mesh->GetAnimations().at(0).duration < time)
            //    {
            //        time = 0; // Repeat playback
            //    }
            //}


            //gltf_model.gltf_mesh->UpdateAnimation(elapsed_time);
            //this->gltf_model.gltf_mesh->Render(dc,comp_mng->Get<ComponentLocalToWorld>(gltf_model.world_id).value );
            //comp_mng->TryGetByEntity<ComponentGltf>(gltf_model.entity)->model
            //    ->UpdateAnimation(elapsed_time);

            render_sys_mng->RenderAll();
        }
        framebuffers_[0]->Deactivate(dc);
    }
    //ポストエフェクト
    {
        framebuffers_[1]->Clear(dc);
        framebuffers_[1]->Activate(dc);
        bit_block_transfer_->blit(dc, framebuffers_[0]->GetShaderResourceView(0).GetAddressOf(), 0, 1, pixel_shaders_[0].Get());
        framebuffers_[1]->Deactivate(dc);

        ID3D11ShaderResourceView* shader_resource_views[2]
        { framebuffers_[0]->GetShaderResourceView(0).Get(),framebuffers_[1]->GetShaderResourceView(0).Get() };
        bit_block_transfer_->blit(dc, shader_resource_views, 0, 2, pixel_shaders_[1].Get());
    }
}

void SceneTest::DrawGui()
{
    float screen_width = Graphics::Instance().GetScreenWidth();
    float screen_height = Graphics::Instance().GetScreenHeight();
    float imgui_window_size_x = 400.f;
    float imgui_window_pos_x = 0.f;
    float imgui_alpha = 0.75f;

    ImGui::SetNextWindowPos({ imgui_window_pos_x,0 }, ImGuiSetCond_Always);
    ImGui::SetNextWindowSize({ imgui_window_size_x,screen_height }, ImGuiSetCond_Always);
    ImGui::SetNextWindowBgAlpha(imgui_alpha);
    ImGui::Begin("scene_test");
    {
        //カメラ操作
        {
            ImGui::Separator();
            ImGui::Text("camera");
            DirectX::XMFLOAT3 camera_eye = camera_.GetEye();
            ImGui::SliderFloat3("eye", &camera_eye.x, -10.0f, 10.f);
            camera_.SetLookAt(camera_eye, camera_.GetFocus(), camera_.GetUp());
        }
        //ライト操作
        {
            ImGui::Separator();
            ImGui::Text("light");
            DirectionLight light = light_manager_.GetDirectionLight();
            bool light_flag = false;
            if (ImGui::SliderFloat4("lihgt_direction", &light.direction.x, -1.f, 1.f))
            {
                light_flag = true;
            }
            if (ImGui::ColorEdit4("light_color", &light.color.x))
            {
                light_flag = true;
            }
            if (light_flag)
            {
                light_manager_.SetDirectionLight(light);
            }
        }

        //オブジェクト
        {
            float limit;
            limit = 5.f;
            //geometric_primitive
            ImGui::Separator();
            ImGui::Text("model");
            ImGui::SliderFloat3("position", &model.position.x, -limit, limit);
            ImGui::SliderFloat3("rotation", &model.rotation.x, -limit, limit);
            ImGui::SliderFloat3("scale", &model.scale.x, -limit, limit);
        }
    }
    ImGui::End();

    comp_edit->DrawImgui();
}
