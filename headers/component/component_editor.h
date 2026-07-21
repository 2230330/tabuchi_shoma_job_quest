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
    void DrawGpuTimeMs(float gpu_frame);
private:
    ComponentManager& comp_mng_;
    EntityManager& enti_mng_;

    int32_t sky_entity_ = -1;
    bool has_sky_ = false;
    int32_t cloud_entity_ = -1;
    bool has_cloud_ = false;
    int32_t cascade_shadow_entity = -1;
    bool has_cascade_shadow_ = false;
    int32_t ssr_entity_ = -1;
    bool has_ssr_ = false;
    int32_t has_deferred_ = -1;
    int32_t has_camera_ = -1;
    int32_t fog_entity_ = -1;
    bool has_fog_ = false;



    //リネーム中のEntity
    uint32_t rename_entity_ = UINT32_MAX;
    char rename_buffer_[256]{};
    bool is_renaming_ = false;
    bool renaming_just_started_ = false;
    bool renaming_ever_active_ = false;
    ImVec2 rename_text_pos_;
    float rename_text_width_ = 0.0f;
};