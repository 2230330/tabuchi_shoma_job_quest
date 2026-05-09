#include "../../headers/component/component_editor.h"
#include<DirectXMath.h>
#include<Windows.h>
#include<string>
#include<filesystem>
#include<fstream>
#include<nlohmann/json.hpp>
#include"../../external/imgui/imgui.h"
#include"../../headers/graphics.h"
#include"../../headers/component/component_manager.h"
#include"../../headers/resource_manager.h"
#include"../../headers/entity/entity_manager.h"
#include"../../headers/system/render_deferred_system.h"


std::wstring ToWString(const std::string& str)
{
    if (str.empty()) return std::wstring();

    int size_needed = MultiByteToWideChar(
        CP_UTF8, 0,
        str.c_str(), (int)str.size(),
        NULL, 0);

    std::wstring wstr(size_needed, 0);

    MultiByteToWideChar(
        CP_UTF8, 0,
        str.c_str(), (int)str.size(),
        &wstr[0], size_needed);

    return wstr;
}


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

        if (ImGui::Button("save"))
        {
            Save("progress.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("load"))
        {
            Load("progress.json");
        }

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
        if (ImGui::Button("atmosphere"))
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
        if (ImGui::Button("cloud"))
        {

            if (has_cloud_ < 0)
            {
                uint32_t entity = enti_mng_.Add();

                ComponentVolumetricCloud cloud_dome;
                comp_mng_.Add(entity, cloud_dome);

                has_cloud_ = entity;
            }
            else
            {
                enti_mng_.Remove(has_cloud_); // alive = false にする
                comp_mng_.RemoveAllComponents(has_cloud_); // すべてのコンポーネントを削除

                has_cloud_ = -1;
            }
        }
        //カスケードシャドウの追加
        if (ImGui::Button("cascade shadow"))
        {
            if (has_cascade_shadow_ < 0)
            {
                uint32_t entity = enti_mng_.Add();

                ComponentCascadeShadow shadow;
                comp_mng_.Add(entity, shadow);

                has_cascade_shadow_ = entity;
            }
            else
            {
                enti_mng_.Remove(has_cascade_shadow_); // alive = false にする
                comp_mng_.RemoveAllComponents(has_cascade_shadow_); // すべてのコンポーネントを削除

                has_cascade_shadow_ = -1;
            }
        }

        //スクリーンスペースリフレクションの追加
        if (ImGui::Button("ssr"))
        {
            if(has_ssr_<0)
            {
                uint32_t entity = enti_mng_.Add();

                ComponentSsr ssr;
                comp_mng_.Add(entity, ssr);

                has_ssr_ = entity;
            }
            else
            {
                enti_mng_.Remove(has_ssr_); // alive = false にする
                comp_mng_.RemoveAllComponents(has_ssr_); // すべてのコンポーネントを削除

                has_ssr_ = -1;
            }
        }


        //生きているエンティティを表示する
        const std::vector<Entity>& entities = enti_mng_.GetArray();
        for (const Entity& entity : entities)
        {
            if (!entity.alive) continue;

            ImGui::PushID(entity.entity);

            // 表示する名前
            std::string label;
            if (comp_mng_.Has<ComponentName>(entity.entity))
            {
                label = comp_mng_.GetByEntity<ComponentName>(entity.entity).value;
            }
            else
            {
                label = "Entity " + std::to_string(entity.entity);
            }

            // 通常表示
            if (!is_renaming_ || rename_entity_ != entity.entity)
            {
                ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
                ImVec2 screen_pos = ImGui::GetCursorScreenPos();

                // 当たり判定
                ImGui::InvisibleButton("rename_hitbox", text_size);

                // 表示（完全に同じ座標）
                ImGui::SetCursorScreenPos(screen_pos);
                ImGui::TextUnformatted(label.c_str());

                // ダブルクリック or 右クリック
                if (
                    (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) ||
                    (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1))
                    )
                {
                    is_renaming_ = true;
                    rename_entity_ = entity.entity;
                    renaming_just_started_ = true;

                    if (comp_mng_.Has<ComponentName>(entity.entity))
                    {
                        strncpy_s(
                            rename_buffer_,
                            comp_mng_.GetByEntity<ComponentName>(entity.entity)
                            .value.c_str(),
                            sizeof(rename_buffer_));
                    }
                    else
                    {
                        snprintf(rename_buffer_, sizeof(rename_buffer_),
                            "Entity %u", entity.entity);
                    }

                    // 入力用の座標と幅を保存
                    rename_text_pos_ = screen_pos;
                    rename_text_width_ = text_size.x + 20.0f;

                    ImGui::SetKeyboardFocusHere();
                }
            }
            // リネーム中
            else

            {
                ImGui::SetCursorScreenPos(rename_text_pos_);
                ImGui::PushItemWidth(rename_text_width_);

                bool commit = ImGui::InputText(
                    "##rename",
                    rename_buffer_,
                    sizeof(rename_buffer_),
                    ImGuiInputTextFlags_EnterReturnsTrue);

                ImGui::PopItemWidth();

                //開始フレームは何もしない
                if (renaming_just_started_)
                {
                    renaming_just_started_ = false;
                    continue; 
                }

                //一度でも Active になったか記録
                if (ImGui::IsItemActive())
                {
                    renaming_ever_active_ = true;
                }

                //Active を経験した後だけ確定判定
                if (renaming_ever_active_ &&
                    (commit || !ImGui::IsItemActive()))
                {
                    if (!comp_mng_.Has<ComponentName>(entity.entity))
                    {
                        ComponentName cn{};
                        comp_mng_.Add(entity.entity, cn);
                    }

                    comp_mng_.GetByEntity<ComponentName>(entity.entity).value
                        = rename_buffer_;

                    is_renaming_ = false;
                    rename_entity_ = UINT32_MAX;
                    renaming_ever_active_ = false;
                }
            }

            ImGui::PopID();


            if (ImGui::TreeNode(label.c_str()))
            {
                ImGui::Separator();
                // 位置
                if (comp_mng_.Has<ComponentPosition>(entity.entity))
                {
                    auto& pos = comp_mng_.GetByEntity<ComponentPosition>(entity.entity);
                    ImGui::DragFloat3("Position", &pos.value.x);
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
                    ImGui::DragFloat3("Scale", &scale.value.x, 0.01f);
                    ImGui::Separator();
                }
                // 色
                if (comp_mng_.Has<ComponentColor>(entity.entity))
                {
                    auto& color = comp_mng_.GetByEntity<ComponentColor>(entity.entity);
                    ImGui::ColorEdit4("Color", &color.value.x);
                    ImGui::Separator();
                }

                //大気散乱調整用
                if (comp_mng_.Has<ComponentSkyAtmosphere>(entity.entity))
                {
                    ImGui::Text("Sky Atmosphere Component");
                    has_sky_ = entity.entity;

                    auto& sky = comp_mng_.Get<ComponentSkyAtmosphere>(entity.entity);

                    ImGui::Separator();
                    ImGui::Text("Scattering Scale Heights");

                    ImGui::DragFloat("Rayleigh Scale Height (m)",
                        &sky.rayleigh_scale_height,
                        100.0f, 1000.0f, 20000.0f, "%.0f");

                    ImGui::DragFloat("Mie Scale Height (m)",
                        &sky.mie_scale_height,
                        50.0f, 100.0f, 10000.0f, "%.0f");

                    ImGui::Separator();
                    ImGui::Text("Ozone Layer");

                    ImGui::DragFloat("Ozone Half Width (m)",
                        &sky.ozone_scale_half_width,
                        500.0f, 1000.0f, 50000.0f, "%.0f");

                    ImGui::DragFloat("Ozone Center Height (m)",
                        &sky.ozone_center_height,
                        1000.0f, 10000.0f, 100000.0f, "%.0f");

                    ImGui::Separator();
                    ImGui::Text("Planet Settings");

                    ImGui::DragFloat("Earth Radius (m)",
                        &sky.earth_height,
                        1000.0f, 6000000.0f, 7000000.0f, "%.0f");

                    ImGui::DragFloat("Atmosphere Height (m)",
                        &sky.atmosphere_height,
                        100.0f, 10000.0f, 200000.0f, "%.0f");

                    ImGui::DragFloat("Sun Distance (m)",
                        &sky.sun_distance,
                        1e7f, 1e9f, 3e11f, "%.0e");

                    ImGui::Separator();
                    ImGui::Text("Sampling");

                    ImGui::SliderInt("Max Sample Count",
                        &sky.max_sample,
                        1, 128);

                    ImGui::Separator();
                }

                //雲のみの処理なので、上の方に置いておきます
                if (comp_mng_.Has<ComponentVolumetricCloud>(entity.entity))
                {
                    auto& c = comp_mng_.GetByEntity<ComponentVolumetricCloud>(entity.entity);

                    if (ImGui::CollapsingHeader("Cloud Ray Marching Settings", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        if(ImGui::Button("shadow_flag"))
                        {
                            c.shadow_flag =!c.shadow_flag;
                        }
                        ImGui::Separator();

                        ImGui::Text("Wind");
                        ImGui::DragFloat2("Wind Direction", reinterpret_cast<float*>(&c.wind_direction), 0.01f, -1.0f, 1.0f);
                        ImGui::DragFloat("Wind Speed", &c.wind_speed, 0.05f, 0.0f, 20.f);

                        ImGui::Separator();

                        ImGui::Text("Altitude");
                        ImGui::DragFloat2("Cloud Altitude Min/Max", reinterpret_cast<float*>(&c.cloud_altitudes_min_max),
                            10.0f, 0.0f, 6383000.0f);

                        ImGui::Separator();

                        ImGui::Text("Density / Coverage");
                        ImGui::SliderFloat("Density Scale", &c.density_scale, 0.01f, 0.5f);
                        ImGui::SliderFloat("Coverage Scale", &c.cloud_coverage_scale, 0.0f, 0.8f);
                        ImGui::SliderFloat("Rain Absorption", &c.rain_cloud_absorption_scale, 0.0f, 10.0f);
                        //ImGui::SliderFloat("Cloud Type Scale", &c.cloud_type_scale, 0.0f, 3.0f);
                        static int cloud_type = 1;
                        ImGui::SliderInt("Cloud Type", &cloud_type, 0, 3);
                        c.cloud_type_scale = static_cast<float>(cloud_type);

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
                // GLTFモデル
                if (comp_mng_.Has<ComponentGltf>(entity.entity))
                {
                    auto& gltf = comp_mng_.GetByEntity<ComponentGltf>(entity.entity);
                    ImGui::Text("GLTF Model");

                    ImGui::Separator();
                    auto& ajast_pbr = comp_mng_.GetByEntity<ComponentAdjastPbrParamter>(entity.entity);

                    ImGui::SliderFloat("Adjust Metalness", &ajast_pbr.adjust_metalness, -1.0f, 1.0f);
                    ImGui::SliderFloat("Adjust Roughness", &ajast_pbr.adjust_roughness, .0f, 1.0f);

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
                // Camera
                if (comp_mng_.Has<ComponentCamera>(entity.entity))
                {
                    auto& cam = comp_mng_.GetByEntity<ComponentCamera>(entity.entity);

                    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::Checkbox("Main Camera", &cam.main_camera_flag_);

                        ImGui::DragFloat3("Position", &cam.camera_position.x, 0.1f);
                        ImGui::DragFloat3("Direction", &cam.camera_direction.x, 0.01f);

                        ImGui::Separator();

                        ImGui::DragFloat("Near Clip", &cam.camera_clip_distance.x, 0.01f, 0.001f, 100.0f);
                        ImGui::DragFloat("Far Clip", &cam.camera_clip_distance.y, 1.0f, 1.0f, 100000.0f);

                        ImGui::Separator();

                        ImGui::Text("View Matrix");
                        ImGui::TextDisabled("Auto calculated");

                        ImGui::Text("Projection Matrix");
                        ImGui::TextDisabled("Auto calculated");
                    }

                    ImGui::Separator();
                }

                //Cascade Shadow
                if (comp_mng_.Has<ComponentCascadeShadow>(entity.entity))
                {
                    auto& shadow = comp_mng_.GetByEntity<ComponentCascadeShadow>(entity.entity);
                    for (int i = 0; i < RenderDeferredSystem::CASCADE::CascadeCount; i++)
                    {
                        ImGui::Image(shadow.srvs_.at(i).Get(), { 256,256 }, { 0,0 });
                    }
                }

                //SSR
                if (comp_mng_.Has<ComponentSsr>(entity.entity))
                {
                    auto& ssr = comp_mng_.GetByEntity<ComponentSsr>(entity.entity);

                    ImGui::Text("Screen Space Reflection");

                    ImGui::DragFloat("Distance", &ssr.distance, 0.1f, 0.1f, 100.0f);
                    ImGui::DragInt("Num Steps", &ssr.num_steps, 1, 1, 128);
                    ImGui::DragInt("Max Mip", &ssr.max_mip, 1, 1, 10);
                    ImGui::DragFloat("Thickness", &ssr.thickness, 0.01f, 0.01f, 1.0f);
                    ImGui::DragFloat("Resolution", &ssr.resolution, 0.01f, 0.01f, 1.0f);
                    ImGui::DragFloat("Start Bias", &ssr.start_bias, 0.01f, 0.0f, 1.0f);
                    ImGui::SliderFloat("Intensity", &ssr.intensity, 0.0f, 10.0f);

                    ImGui::Image(ssr.ssr_texture.Get(), { 256,256, }, { 0,0 });
                    ImGui::Image(ssr.normal.Get(), { 256,256, }, { 0,0 });
                    ImGui::Image(ssr.color.Get(), { 256,256, }, { 0,0 });
                    ImGui::Image(ssr.depth.Get(), { 256,256, }, { 0,0 });

                    ImGui::Separator();
                }

                //コンポーネントの追加
                if (has_sky_!=entity.entity|| !has_cloud_!=entity.entity)
                {
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
                                                    ComponentAdjastPbrParamter ajast_pbr{};
                                                    comp_mng_.Add(entity.entity, ajast_pbr);
                                                }

                                            }
                                        }
                                    }
                                    ImGui::TreePop();
                                }
                            }
                            if (!comp_mng_.Has<ComponentTexture>(entity.entity)) {

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

                        ImGui::TreePop();
                    }
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

void ComponentEditor::Save(const std::string& filename)
{
    using json = nlohmann::json;

    json root;
    root["entities"] = json::array();

    for (const auto& entity : enti_mng_.GetArray())
    {
        if (!entity.alive) continue;

        json entity_json;
        json comp_json;

        entity_json["entity_id"] = entity.entity;

        // =========================
        // Transform
        // =========================
        if (comp_mng_.Has<ComponentPosition>(entity.entity))
        {
            auto& c = comp_mng_.GetByEntity<ComponentPosition>(entity.entity);
            comp_json["Position"] = { {"x",c.value.x},{"y",c.value.y},{"z",c.value.z} };
        }

        if (comp_mng_.Has<ComponentRotation>(entity.entity))
        {
            auto& c = comp_mng_.GetByEntity<ComponentRotation>(entity.entity);
            comp_json["Rotation"] = { {"x",c.value.x},{"y",c.value.y},{"z",c.value.z} };
        }

        if (comp_mng_.Has<ComponentScale>(entity.entity))
        {
            auto& c = comp_mng_.GetByEntity<ComponentScale>(entity.entity);
            comp_json["Scale"] = { {"x",c.value.x},{"y",c.value.y},{"z",c.value.z} };
        }

        // =========================
        // Color
        // =========================
        if (comp_mng_.Has<ComponentColor>(entity.entity))
        {
            auto& c = comp_mng_.GetByEntity<ComponentColor>(entity.entity);
            comp_json["Color"] = {
                {"r",c.value.x},{"g",c.value.y},
                {"b",c.value.z},{"a",c.value.w}
            };
        }

        // =========================
        // Sky Atmosphere（全項目）
        // =========================
        if (comp_mng_.Has<ComponentSkyAtmosphere>(entity.entity))
        {
            auto& s = comp_mng_.GetByEntity<ComponentSkyAtmosphere>(entity.entity);

            comp_json["SkyAtmosphere"] =
            {
                {"rayleigh_scale_height",s.rayleigh_scale_height},
                {"mie_scale_height",s.mie_scale_height},
                {"ozone_scale_half_width",s.ozone_scale_half_width},
                {"ozone_center_height",s.ozone_center_height},
                {"earth_height",s.earth_height},
                {"atmosphere_height",s.atmosphere_height},
                {"sun_distance",s.sun_distance},
                {"max_sample",s.max_sample}
            };
        }

        // =========================
        // Volumetric Cloud（全項目）
        // =========================
        if (comp_mng_.Has<ComponentVolumetricCloud>(entity.entity))
        {
            auto& c = comp_mng_.GetByEntity<ComponentVolumetricCloud>(entity.entity);

            comp_json["VolumetricCloud"] =
            {
                {"shadow_flag",c.shadow_flag},
                {"wind_x",c.wind_direction.x},
                {"wind_y",c.wind_direction.y},
                {"wind_speed",c.wind_speed},

                {"alt_min",c.cloud_altitudes_min_max.x},
                {"alt_max",c.cloud_altitudes_min_max.y},

                {"density_scale",c.density_scale},
                {"coverage_scale",c.cloud_coverage_scale},
                {"rain_absorption",c.rain_cloud_absorption_scale},
                {"cloud_type_scale",c.cloud_type_scale},

                {"earth_radius",c.earth_radius},
                {"horizon_scale",c.horizon_distance_scale},

                {"low_freq_scale",c.low_frequency_perlin_worley_sampling_scale},
                {"high_freq_scale",c.high_frequency_worley_sampling_scale},
                {"long_distance_density",c.cloud_density_long_distance_scale},

                {"powdered_sugar",c.enable_powdered_sugar_efffect},

                {"ray_march_steps",c.ray_marching_steps},
                {"auto_ray_march",c.auto_ray_marching_steps}
            };
        }

        // =========================
        // GLTF
        // =========================
        if (comp_mng_.Has<ComponentGltf>(entity.entity))
        {
            auto& g = comp_mng_.GetByEntity<ComponentGltf>(entity.entity);

            comp_json["Gltf"] =
            {
                {"filename", g.model ? g.model->GetFilename() : ""},
                {"animation_time", g.animation_time},
                {"animation_index", g.animation_index}
            };
        }

        // =========================
        // PBR
        // =========================
        if (comp_mng_.Has<ComponentAdjastPbrParamter>(entity.entity))
        {
            auto& p = comp_mng_.GetByEntity<ComponentAdjastPbrParamter>(entity.entity);

            comp_json["AdjustPBR"] =
            {
                {"metalness",p.adjust_metalness},
                {"roughness",p.adjust_roughness}
            };
        }

        // =========================
        // Texture
        // =========================
        if (comp_mng_.Has<ComponentTexture>(entity.entity))
        {
            auto& t = comp_mng_.GetByEntity<ComponentTexture>(entity.entity);
            comp_json["Texture"] = { {"name",t.name} };
        }

        // =========================
        // Instanced
        // =========================
        if (comp_mng_.Has<ComponentInstanced>(entity.entity))
        {
            comp_json["Instanced"] = true;
        }

        // =========================
        // Camera
        // =========================
        if (comp_mng_.Has<ComponentCamera>(entity.entity))
        {
            auto& c = comp_mng_.GetByEntity<ComponentCamera>(entity.entity);

            comp_json["Camera"] =
            {
                {"pos_x", c.camera_position.x},
                {"pos_y", c.camera_position.y},
                {"pos_z", c.camera_position.z},

                {"dir_x", c.camera_direction.x},
                {"dir_y", c.camera_direction.y},
                {"dir_z", c.camera_direction.z},

                {"near", c.camera_clip_distance.x},
                {"far", c.camera_clip_distance.y},

                {"main", c.main_camera_flag_}
            };
        }

        // =========================
        // Cascade Shadow
        // =========================
        if (comp_mng_.Has<ComponentCascadeShadow>(entity.entity))
        {
            comp_json["Cascade Shadow"] = true;
        }

        // =========================
        // SSR
        // =========================
        if (comp_mng_.Has<ComponentSsr>(entity.entity))
        {
            auto& ssr = comp_mng_.GetByEntity<ComponentSsr>(entity.entity);

            comp_json["ScreenSpaceReflection"] =
            {
                {"distance", ssr.distance},
                {"num_steps", ssr.num_steps},
                {"max_mip", ssr.max_mip},
                {"thickness", ssr.thickness},
                {"resolution", ssr.resolution},
                {"start_bias", ssr.start_bias},
                {"intensity",ssr.intensity}
            };


        }

        if (comp_mng_.Has<ComponentName>(entity.entity))
        {
            auto& n = comp_mng_.GetByEntity<ComponentName>(entity.entity);
            comp_json["Name"] = 
            {
                {"value", n.value}
            };
        
        }


        entity_json["components"] = comp_json;
        root["entities"].push_back(entity_json);
    }

    std::ofstream file(filename);
    if (file.is_open())
        file << root.dump(4);
}

void ComponentEditor::Load(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) return;

    using json = nlohmann::json;
    json root;
    file >> root;

    has_sky_ = -1;
    has_cloud_ = -1;

    // 既存削除
    for (auto& e : enti_mng_.GetArray())
    {
        if (e.alive)
        {
            comp_mng_.RemoveAllComponents(e.entity);
            enti_mng_.Remove(e.entity);
        }
    }

    for (auto& entity_json : root["entities"])
    {
        uint32_t entity = entity_json["entity_id"];
        enti_mng_.AddWithID(entity);

        auto& comp_json = entity_json["components"];

        // Transform
        if (comp_json.contains("Position"))
        {
            ComponentPosition c;
            auto& j = comp_json["Position"];
            c.value = { j["x"],j["y"],j["z"] };
            comp_mng_.Add(entity, c);
        }

        if (comp_json.contains("Rotation"))
        {
            ComponentRotation c;
            auto& j = comp_json["Rotation"];
            c.value = { j["x"],j["y"],j["z"] };
            comp_mng_.Add(entity, c);
        }

        if (comp_json.contains("Scale"))
        {
            ComponentScale c;
            auto& j = comp_json["Scale"];
            c.value = { j["x"],j["y"],j["z"] };
            comp_mng_.Add(entity, c);
        }

        if (!comp_mng_.Has<ComponentLocalToWorld>(entity))
        {
            ComponentLocalToWorld l2w{};
            comp_mng_.Add(entity, l2w);
        }

        // Color
        if (comp_json.contains("Color"))
        {
            ComponentColor c;
            auto& j = comp_json["Color"];
            c.value = { j["r"],j["g"],j["b"],j["a"] };
            comp_mng_.Add(entity, c);
        }

        // Sky
        if (comp_json.contains("SkyAtmosphere"))
        {
            ComponentSkyAtmosphere s;
            auto& j = comp_json["SkyAtmosphere"];

            s.rayleigh_scale_height = j["rayleigh_scale_height"];
            s.mie_scale_height = j["mie_scale_height"];
            s.ozone_scale_half_width = j["ozone_scale_half_width"];
            s.ozone_center_height = j["ozone_center_height"];
            s.earth_height = j["earth_height"];
            s.atmosphere_height = j["atmosphere_height"];
            s.sun_distance = j["sun_distance"];
            s.max_sample = j["max_sample"];

            comp_mng_.Add(entity, s);
            has_sky_ = entity;
        }

        // Cloud
        if (comp_json.contains("VolumetricCloud"))
        {
            ComponentVolumetricCloud c;
            auto& j = comp_json["VolumetricCloud"];

            c.shadow_flag = j["shadow_flag"];
            c.wind_direction = { j["wind_x"],j["wind_y"] };
            c.wind_speed = j["wind_speed"];

            c.cloud_altitudes_min_max = { j["alt_min"],j["alt_max"] };

            c.density_scale = j["density_scale"];
            c.cloud_coverage_scale = j["coverage_scale"];
            c.rain_cloud_absorption_scale = j["rain_absorption"];
            c.cloud_type_scale = j["cloud_type_scale"];

            c.earth_radius = j["earth_radius"];
            c.horizon_distance_scale = j["horizon_scale"];

            c.low_frequency_perlin_worley_sampling_scale = j["low_freq_scale"];
            c.high_frequency_worley_sampling_scale = j["high_freq_scale"];
            c.cloud_density_long_distance_scale = j["long_distance_density"];

            c.enable_powdered_sugar_efffect = j["powdered_sugar"];

            c.ray_marching_steps = j["ray_march_steps"];
            c.auto_ray_marching_steps = j["auto_ray_march"];

            comp_mng_.Add(entity, c);
            has_cloud_ = entity;
        }

        // GLTF
        if (comp_json.contains("Gltf"))
        {
            auto& j = comp_json["Gltf"];
            std::string filename = j["filename"];

            auto model = ResourceManager::Instance().LoadGltfModel(Graphics::Instance().GetDevice(), filename);

            if (model)
            {
                ComponentGltf g;
                g.model = model;
                g.animation_time = j["animation_time"];
                g.animation_index = j["animation_index"];
                comp_mng_.Add(entity, g);

                ComponentAdjastPbrParamter p{};
                comp_mng_.Add(entity, p);
            }
        }

        // PBR
        if (comp_json.contains("AdjustPBR"))
        {
            ComponentAdjastPbrParamter p;
            auto& j = comp_json["AdjustPBR"];
            p.adjust_metalness = j["metalness"];
            p.adjust_roughness = j["roughness"];
            comp_mng_.Add(entity, p);
        }

        // Texture
        if (comp_json.contains("Texture"))
        {
            ComponentTexture t;
            t.name = comp_json["Texture"]["name"];
            t.texture =
                ResourceManager::Instance().LoadTextureFromFile(Graphics::Instance().GetDevice(), ToWString(t.name).c_str());
            comp_mng_.Add(entity, t);
        }

        // Instanced
        if (comp_json.contains("Instanced"))
        {
            ComponentInstanced i;
            comp_mng_.Add(entity, i);
        }

        // Camera
        if (comp_json.contains("Camera"))
        {
            ComponentCamera c;
            auto& j = comp_json["Camera"];

            c.camera_position = { j["pos_x"], j["pos_y"], j["pos_z"], 1.0f };
            c.camera_direction = { j["dir_x"], j["dir_y"], j["dir_z"], 0.0f };
            c.camera_clip_distance = { j["near"], j["far"], 0, 0 };

            c.main_camera_flag_ = j["main"];

            comp_mng_.Add(entity, c);
        }

        //Cascade Shadow
        if (comp_json.contains("Cascade Shadow"))
        {
            ComponentCascadeShadow shadow;
            comp_mng_.Add(entity, shadow);
        }

        // SSR
        if (comp_json.contains("ScreenSpaceReflection"))
        {
            ComponentSsr ssr;
            auto& j = comp_json["ScreenSpaceReflection"];
            ssr.distance = j["distance"];
            ssr.num_steps = j["num_steps"];
            ssr.max_mip = j["max_mip"];
            ssr.thickness = j["thickness"];
            ssr.resolution = j["resolution"];
            ssr.start_bias = j["start_bias"];
            ssr.intensity = j["intensity"];
            comp_mng_.Add(entity, ssr);
            has_ssr_ = entity;
        }
        // Name
        if (comp_json.contains("Name"))
        {
            ComponentName n;
            n.value = comp_json["Name"]["value"];
            comp_mng_.Add(entity, n);

        }
    }
}
