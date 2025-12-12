#include "../../headers/component/component_editor.h"
#include<DirectXMath.h>
#include<Windows.h>
#include<string>
#include"../../external/imgui/imgui.h"

ComponentEditor::ComponentEditor(
    ComponentManager& component_manager
    , EntityManager& entity_manager)
    :comp_mng_(component_manager)
    ,enti_mng_(entity_manager)
{
}

void ComponentEditor::DrawImgui()
{
    if (ImGui::Begin("Component Editor"))
    {

        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        // エンティティ追加ボタン
        if (ImGui::Button("Add Entity"))
        {
            uint32_t entity=enti_mng_.Add();

            //基礎情報の追加
            ComponentPosition position{};
            comp_mng_.Add<ComponentPosition>(entity,position);
            ComponentRotation rotation{};
            comp_mng_.Add<ComponentRotation>(entity, rotation);
            ComponentScale scale;
            scale.value={ 1.f, 1.f, 1.f };
            comp_mng_.Add(entity, scale);
            ComponentLocalToWorld l2w{};
            comp_mng_.Add(entity, l2w);
        }

        //大気の追加
        if (ImGui::Button("add atmosphere"))
        {

            if (has_sky_<0)
            {
                uint32_t entity = enti_mng_.Add();
                //基礎情報の追加
                ComponentPosition position{};
                comp_mng_.Add<ComponentPosition>(entity, position);
                ComponentRotation rotation{};
                comp_mng_.Add<ComponentRotation>(entity, rotation);
                ComponentScale scale;
                scale.value = { 1.f, 1.f, 1.f };
                comp_mng_.Add(entity, scale);
                ComponentLocalToWorld l2w{};
                comp_mng_.Add(entity, l2w);
                ComponentSkyAtmosphere sky;
                comp_mng_.Add(entity, sky);

                has_sky_ = entity;
            }
            else
            {
                enti_mng_.Remove(has_sky_); // alive = false にする
                comp_mng_.RemoveAllComponents(has_sky_); // すべてのコンポーネントを削除

                has_sky_ = -1;
            }
        }
        //雲の追加
        if (ImGui::Button("add cloud"))
        {

            if (has_cloud_ < 0)
            {
                uint32_t entity = enti_mng_.Add();
                //基礎情報の追加
                //ComponentPosition position{};
                //comp_mng_.Add<ComponentPosition>(entity, position);
                //ComponentRotation rotation{};
                //comp_mng_.Add<ComponentRotation>(entity, rotation);
                //ComponentScale scale;
                //scale.value = { 1.f, 1.f, 1.f };
                //comp_mng_.Add(entity, scale);
                //ComponentLocalToWorld l2w{};
                //comp_mng_.Add(entity, l2w);
                //ComponentTexture texture;
                //comp_mng_.Add(entity, texture);
                ComponentCloudDome cloud_dome;
                comp_mng_.Add(entity, cloud_dome);
                //ComponentVolumetricCloud volumetric_cloud;
                //comp_mng_.Add(entity, volumetric_cloud);

                has_cloud_ = entity;
            }
            else
            {
                enti_mng_.Remove(has_cloud_); // alive = false にする
                comp_mng_.RemoveAllComponents(has_cloud_); // すべてのコンポーネントを削除

                has_cloud_ = -1;
            }
        }

        //生きているエンティティを表示する
        const std::vector<Entity>& entities = enti_mng_.GetArray();
        for (const Entity& entity : entities)
        {
            if (!entity.alive) continue;

            //エディタ毎に表示
            std::string label = "Entity " + std::to_string(entity.entity);
            if (ImGui::TreeNode(label.c_str()))
            {
                ImGui::Separator();
                // 位置
                if (comp_mng_.Has<ComponentPosition>(entity.entity))
                {
                    auto& pos = comp_mng_.GetByEntity<ComponentPosition>(entity.entity);
                    ImGui::SliderFloat3("Position", &pos.value.x, -10.f, 10.f);
                    ImGui::Separator();
                }
                // 回転
                if (comp_mng_.Has<ComponentRotation>(entity.entity))
                {
                    auto& rot = comp_mng_.GetByEntity<ComponentRotation>(entity.entity);
                    ImGui::SliderFloat3("Rotation", &rot.value.x, -3.14f,3.14f);
                    ImGui::Separator();
                }
                // スケール
                if (comp_mng_.Has<ComponentScale>(entity.entity))
                {
                    auto& scale = comp_mng_.GetByEntity<ComponentScale>(entity.entity);
                    ImGui::SliderFloat3("Scale", &scale.value.x, 0.01f,10.0f);
                    ImGui::Separator();
                }
                // 色
                if (comp_mng_.Has<ComponentColor>(entity.entity))
                {
                    auto& color = comp_mng_.GetByEntity<ComponentColor>(entity.entity);
                    ImGui::ColorEdit4("Color", &color.value.x);
                    ImGui::Separator();
                }
                //雲のみの処理なので、上の方に置いておきます
                if (comp_mng_.Has<ComponentCloudDome>(entity.entity))
                {
                    auto& c = comp_mng_.GetByEntity<ComponentCloudDome>(entity.entity);

                    if (ImGui::CollapsingHeader("Cloud Ray Marching Settings", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::Text("Wind");
                        ImGui::DragFloat2("Wind Direction", reinterpret_cast<float*>(&c.wind_direction), 0.01f, -1.0f, 1.0f);
                        ImGui::DragFloat("Wind Speed", &c.wind_speed, 0.05f, 0.0f, 100.0f);

                        ImGui::Separator();

                        ImGui::Text("Altitude");
                        ImGui::DragFloat2("Cloud Altitude Min/Max", reinterpret_cast<float*>(&c.cloud_altitudes_min_max),
                            10.0f, 0.0f, 6000000.0f);

                        ImGui::Separator();

                        ImGui::Text("Density / Coverage");
                        ImGui::SliderFloat("Density Scale", &c.density_scale, 0.01f, 0.5f);
                        ImGui::SliderFloat("Coverage Scale", &c.cloud_coverage_scale, 0.1f, 1.0f);
                        ImGui::SliderFloat("Rain Absorption", &c.rain_cloud_absorption_scale, 0.0f, 100.0f);
                        ImGui::SliderFloat("Cloud Type Scale", &c.cloud_type_scale, 0.0f, 3.0f);

                        ImGui::Separator();

                        ImGui::Text("Ray March / Planet");
                        ImGui::DragFloat("Earth Radius", &c.earth_radius, 1000.0f, 1000000.0f, 20000000.0f);
                        ImGui::SliderFloat("Horizon Distance Scale", &c.horizon_distance_scale, 0.0f, 3.0f);

                        ImGui::Separator();

                        ImGui::Text("Noise Sampling");
                        ImGui::DragFloat("Low Freq Sampling Scale", &c.low_frequency_perlin_worley_sampling_scale,
                            0.00001f, 0.000001f, 0.01f, "%.8f");
                        ImGui::DragFloat("High Freq Sampling Scale", &c.high_frequency_worley_sampling_scale,
                            0.0001f, 0.000001f, 0.01f, "%.8f");

                        ImGui::SliderFloat("Long Distance Density Scale", &c.cloud_density_long_distance_scale, 0.1f, 30.0f);

                        ImGui::Separator();

                        ImGui::Checkbox("Powdered Sugar Effect", reinterpret_cast<bool*>(&c.enable_powdered_sugar_efffect));

                        ImGui::Separator();

                        ImGui::Text("Ray March Quality");
                        ImGui::SliderInt("Ray March Steps", &c.ray_marching_steps, 8, 512);
                        ImGui::Checkbox("Auto Ray March Steps", reinterpret_cast<bool*>(&c.auto_ray_marching_steps));
                    }

                    ImGui::Separator();
                }
                //強度
                if (comp_mng_.Has<ComponentIntensity>(entity.entity))
                {
                    auto& intensity = comp_mng_.GetByEntity<ComponentIntensity>(entity.entity);
                    ImGui::SliderFloat("Intensity", &intensity.value, 0.1f, 100.f);
                    ImGui::Separator();
                }
                // GLTFモデル
                if (comp_mng_.Has<ComponentGltf>(entity.entity))
                {
                    auto& gltf = comp_mng_.GetByEntity<ComponentGltf>(entity.entity);
                    ImGui::Text("GLTF Model");

                    //インスタンシング描画
                    if (ImGui::Button("instanced"))
                    {
                        if (!comp_mng_.Has<ComponentInstanced>(entity.entity))
                        {
                            ComponentInstanced instance;
                            comp_mng_.Add(entity.entity, instance);
                        }
                        else
                        {
                            comp_mng_.Remove<ComponentInstanced>(entity.entity);
                        }
                    }
                    ImGui::Separator();

                    ImGui::Text("Filename: %s", gltf.model->GetFilename().c_str());
                    ImGui::DragFloat("Animation Time", &gltf.animation_time, 0.01f);
                    ImGui::DragInt("Animation Index", reinterpret_cast<int*>(&gltf.animation_index), 1, 0, static_cast<int>(gltf.model->GetAnimations().size()) - 1);
                    ImGui::Separator();
                }
                if (comp_mng_.Has<ComponentTexture>(entity.entity))
                {
                    auto& texture = comp_mng_.GetByEntity<ComponentTexture>(entity.entity);
                    if (ImGui::TreeNode("Texture Model"))
                    {
                        if (ImGui::CollapsingHeader("Select Texture"))
                        {
                            const auto& texs = ResourceManager::Instance().GetTextures();
                            if (texs.empty())
                            {
                                ImGui::Text("No texture loaded.");
                            }
                            else
                            {
                                for (const auto& [name, tex_ptr] : texs)
                                {
                                    int size = WideCharToMultiByte(CP_UTF8, 0,
                                        name.data(), static_cast<int>(name.size()), nullptr, 0, nullptr, nullptr);
                                    std::string tex_name;
                                    tex_name.resize(size);
                                    WideCharToMultiByte(CP_UTF8, 0,
                                        name.data(), static_cast<int>(name.size()), tex_name.data(), size, nullptr, nullptr);
                                    if (ImGui::Button(tex_name.c_str()))
                                    {
                                        if (!tex_ptr)
                                        {
                                            ImGui::Text("texture ptr is null.");
                                        }
                                        else
                                        {
                                            texture.texture = tex_ptr;
                                            texture.name = tex_name;
                                            break;
                                        }
                                    }
                                }
                            }
                        }

                        if (texture.texture != nullptr)
                        {
                            ImGui::Text("Filename: %s", texture.name.c_str());
                            ImGui::Image(texture.texture.Get(), { 256,256, }, { 0,0 });
                        }

                        ImGui::Separator();
                            ImGui::TreePop();
                    }
                    ImGui::Separator();
                }
                if (comp_mng_.Has<ComponentInstanced>(entity.entity))
                {
                    ImGui::Text("Instancing Render");
                    ImGui::Separator();
                }

                //コンポーネントの追加
                if (ImGui::TreeNode("Add Component"))
                {

                    //モデル関係の取
                    {
                        if (!comp_mng_.Has<ComponentGltf>(entity.entity))
                        {
                            if (ImGui::TreeNode("GLTF Model"))
                            {
                                const auto& models = ResourceManager::Instance().GetGltfs();
                                //もしGLTFのモデルが空なら
                                if (models.empty())
                                {
                                    ImGui::Text("No models loaded.");
                                }
                                else
                                {
                                    for (const auto& [name, model_ptr] : models)
                                    {
                                        if (ImGui::Button(name.c_str()))
                                        {
                                            if (!model_ptr) {
                                                ImGui::Text("Model ptr is null.");
                                            }
                                            else
                                            {
                                                ComponentGltf gltf{};
                                                gltf.model = model_ptr;
                                                comp_mng_.Add(entity.entity, gltf);
                                            }

                                        }
                                    }
                                }
                                ImGui::TreePop();
                            }
                        }
                        if (!comp_mng_.Has<ComponentTexture>(entity.entity)){
                            if (ImGui::Button("Add Texture"))
                            {
                                ComponentTexture tex_comp{};
                                tex_comp.texture = nullptr;
                                tex_comp.name = "";
                                comp_mng_.Add(entity.entity, tex_comp);
                                break;
                            }

                        }

                    }
                    {
                        if (!comp_mng_.Has<ComponentColor>(entity.entity))
                        {
                            if(ImGui::Button("Color component"))
                            {
                                ComponentColor color;
                                comp_mng_.Add(entity.entity, color);
                            }
                        }
                    }

                    ImGui::TreePop();
                }

                // エンティティ削除ボタン
                if (ImGui::Button("Delete Entity"))
                {
                    enti_mng_.Remove(entity.entity); // alive = false にする
                    comp_mng_.RemoveAllComponents(entity.entity); // すべてのコンポーネントを削除

                    if (has_sky_ == entity.entity)has_sky_ = -1;
                    else if (has_cloud_ == entity.entity)has_cloud_ = -1;
                }

                ImGui::TreePop();
            }
        }
        ImGui::End();
    }
}