#include "../../headers/component/component_editor.h"
#include<DirectXMath.h>
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

        //生きているエンティティを表示する
        const std::vector<Entity>& entities = enti_mng_.GetArray();
        for (const Entity& entity : entities)
        {
            if (!entity.alive) continue;

            //エディタ毎に表示
            std::string label = "Entity " + std::to_string(entity.entity);
            if (ImGui::TreeNode(label.c_str()))
            {
                // 位置
                if (comp_mng_.Has<ComponentPosition>(entity.entity))
                {
                    auto& pos = comp_mng_.GetByEntity<ComponentPosition>(entity.entity);
                    ImGui::SliderFloat3("Position", &pos.value.x, -10.f, 10.f);
                }
                // 回転
                if (comp_mng_.Has<ComponentRotation>(entity.entity))
                {
                    auto& rot = comp_mng_.GetByEntity<ComponentRotation>(entity.entity);
                    ImGui::SliderFloat3("Rotation", &rot.value.x, -3.14f,3.14f);
                }
                // スケール
                if (comp_mng_.Has<ComponentScale>(entity.entity))
                {
                    auto& scale = comp_mng_.GetByEntity<ComponentScale>(entity.entity);
                    ImGui::SliderFloat3("Scale", &scale.value.x, 0.1f,10.0f);
                }
                // 色
                if (comp_mng_.Has<ComponentColor>(entity.entity))
                {
                    auto& color = comp_mng_.GetByEntity<ComponentColor>(entity.entity);
                    ImGui::ColorEdit3("Color", &color.value.x);
                }
                // GLTFモデル
                if (comp_mng_.Has<ComponentGltf>(entity.entity))
                {
                    auto& gltf = comp_mng_.GetByEntity<ComponentGltf>(entity.entity);
                    ImGui::Text("GLTF Model");
                    ImGui::Text("Filename: %s", gltf.model->GetFilename().c_str());
                    ImGui::DragFloat("Animation Time", &gltf.animation_time, 0.01f);
                    ImGui::DragInt("Animation Index", reinterpret_cast<int*>(&gltf.animation_index), 1, 0, static_cast<int>(gltf.model->GetAnimations().size()) - 1);
                }


                //コンポーネントの追加
                if (ImGui::TreeNode("Add Component"))
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


                    ImGui::TreePop();
                }

                // エンティティ削除ボタン
                if (ImGui::Button("Delete Entity"))
                {
                    enti_mng_.Remove(entity.entity); // alive = false にする
                    comp_mng_.RemoveAllComponents(entity.entity); // すべてのコンポーネントを削除
                }

                ImGui::TreePop();
            }
        }
        ImGui::End();
    }
}