#pragma once
#include<cstdint>
#include<string>

#include"../../external/imgui/imgui.h"

//前方宣言
class ComponentManager;
class EntityManager;



//ECSを参考に、コンポーネントとエンティティを使ってデータを管理しています。
//コンポーネントマネージャに収録されているデータを閲覧します。
//ある程度操作もできるようにしています。
class ComponentEditor
{
public:
    ComponentEditor(
        ComponentManager& component_manager,
        EntityManager&entity_manager);
    //�ҏW�p
    void DrawImgui();

    void Save(const std::string& filename);
    void Load(const std::string& filename);
private:
    ComponentManager& comp_mng_;
    EntityManager& enti_mng_;

    int32_t has_sky_ = -1;
    int32_t has_cloud_ = -1;
    int32_t has_cascade_shadow_ = -1;
    int32_t has_ssr_ = -1;
    int32_t has_deferred_ = -1;
    int32_t has_camera_ = -1;


    //リネーム中のEntity
    uint32_t rename_entity_ = UINT32_MAX;
    char rename_buffer_[256]{};
    bool is_renaming_ = false;
    bool renaming_just_started_ = false;
    bool renaming_ever_active_ = false;
    ImVec2 rename_text_pos_;
    float rename_text_width_ = 0.0f;
};