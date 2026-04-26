#pragma once
#include"component_manager.h"
#include"../entity/entity_manager.h"
#include"../resource_manager.h"

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
    int32_t has_ssr_ = -1;

};