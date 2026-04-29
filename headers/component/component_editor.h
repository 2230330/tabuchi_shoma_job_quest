#pragma once
#include<cstdint>
#include<string>
#include"../../external/imgui/imgui.h"

class ComponentManager;
class EntityManager;

//このクラスは、コンポーネントマネージャーの情報を元に
//コンポーネントの確認と追加、削除等を行うために作成されたクラスである。
class ComponentEditor
{
public:
    ComponentEditor(
        ComponentManager& component_manager,
        EntityManager&entity_manager);
    //編集用
    void DrawImgui();

    void Save(const std::string& filename);
    void Load(const std::string& filename);
private:
    ComponentManager& comp_mng_;
    EntityManager& enti_mng_;

    int32_t has_sky_ = -1;
    int32_t has_cloud_ = -1;

    //リネーム中のEntity
    uint32_t rename_entity_ = UINT32_MAX;
    char rename_buffer_[256]{};
    bool is_renaming_ = false;
    bool renaming_just_started_ = false;
    bool renaming_ever_active_ = false;
    ImVec2 rename_text_pos_{};
    float rename_text_width_ = 0.0f;
};