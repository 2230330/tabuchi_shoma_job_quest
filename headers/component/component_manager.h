#pragma once
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <typeindex>
#include <stdexcept>
#include<functional>
#include<tuple>
#include<cassert>
#include<algorithm>

#include "component_position.h"
#include "component_rotation.h"
#include "component_scale.h"
#include "component_local_to_world.h"
#include "component_color.h"
#include "component_gltf.h"
#include "component_mesh.h"
#include "component_primitive.h"
#include "component_material.h"
#include "component_texture.h"
#include "component_instanced.h"
#include "component_volumetric_cloud.h"
#include "component_sky_atmosphere.h"
#include"component_ajast_pbr_paramter_.h"
#include "component_camera.h"
#include "component_screen_space_reflection.h"
#include"component_name.h"
#include"component_cascade_shadow.h"
#include"component_deferred_render.h"
#include"component_bound_box.h"

//コンポーネントの管理者。これからぶくぶく大きくなると考えるとちょっと悩み物
class ComponentManager
{
public:
    ComponentManager() {
        // コンストラクタで型ごとのストレージを登録しておく
        registerContainer<ComponentPosition>(positions_);
        registerContainer<ComponentRotation>(rotations_);
        registerContainer<ComponentScale>(scales_);
        registerContainer<ComponentLocalToWorld>(l2ws_);
        registerContainer<ComponentColor>(colors_);
        registerContainer<ComponentGltf>(gltfs_);
        registerContainer<ComponentMesh>(meshes_);
        registerContainer<ComponentPrimitive>(primitives_);
        registerContainer<ComponentMaterial>(materials_);
        registerContainer<ComponentTexture>(textures_);
        registerContainer<ComponentInstanced>(instanced_);
        registerContainer<ComponentSkyAtmosphere>(skys_);
        registerContainer<ComponentVolumetricCloud>(clouds_);
        registerContainer<ComponentAdjastPbrParamter>(ajast_pbr_paramters_);
        registerContainer<ComponentCamera>(cameras_);
        registerContainer<ComponentSsr>(ssrs_);
        registerContainer<ComponentName>(names_);
        registerContainer<ComponentCascadeShadow>(cas_shadows_);
        registerContainer<ComponentDeferredRender>(deferred_renders_);
        registerContainer<ComponentBoundingBox>(bounding_boxes_);
    }

    template<typename T>
    static const std::type_index& TypeIndex() {
        static const std::type_index type_index(typeid(T));
        return type_index;
    }


    template<typename T>
    int Add(uint32_t entity_id, const T& component)
    {
        const auto& type = TypeIndex<T>();

        auto& mapping = entity_to_component_[type];

        // 同じEntityに同じComponentを二重追加しない
        assert(mapping.find(entity_id) == mapping.end());

        auto& container = getContainer<T>();
        container.emplace_back(component);

        int id = static_cast<int>(container.size() - 1);

        mapping[entity_id] = id;
        component_to_entity_[type].emplace_back(entity_id);

        return id;
    }

    //コンポーネントを格納している配列の要素で取り出すゲッター
    template<typename T>
    T& Get(int id) {
        auto& container = getContainer<T>();
        return container.at(id);
    }

    template<typename T>
    const T& Get(int id) const {
        const auto& container = getContainer<T>();
        return container.at(id);
    }

    //要素の削除関数
    template<typename T>
    void Remove(uint32_t entity_id) {
        const auto& type = TypeIndex<T>();

        // entity_id からコンポーネントのインデックスを取得
        auto mit = entity_to_component_.find(type);
        //なければ何もしない
        if (mit == entity_to_component_.end()) return;

        auto& mapping = mit->second;

        auto it = mapping.find(entity_id);
        if (it == mapping.end()) return;

        auto& container = getContainer<T>();
        auto entity_list_it = component_to_entity_.find(type);
        if (entity_list_it == component_to_entity_.end()) return;

        auto& entities = entity_list_it->second;

        assert(container.size() == entities.size());

        const int index_to_remove = it->second;
        const int last_index = static_cast<int>(container.size() - 1);

        if (index_to_remove != last_index) {
            //最後のコンポーネントを削除位置へ移動
            std::swap(container[index_to_remove], container[last_index]);
            //最後のコンポーネントを持っていてたエンティティを取得
            const uint32_t moved_entity = entities[last_index];
            //entityリスト側も同じ位置へ移動
            entities[index_to_remove] = moved_entity;
            //移動したentityのindex を更新
            mapping[moved_entity] = index_to_remove; 
        }

        container.pop_back();
        entities.pop_back();

        mapping.erase(entity_id);
    }

    //一緒に登録したエンティティで要素を取り出すゲッター
    template<typename T>
    T& GetByEntity(uint32_t entity_id) {
        auto& container = getContainer<T>();
        const auto& mapping = entity_to_component_[TypeIndex<T>()];
        return container.at(mapping.at(entity_id));
    }

    //登録したコンポーネントがない場合などの安全版ゲッター
    template<typename T>
    T* TryGetByEntity(uint32_t entity_id) {
        auto it = entity_to_component_.find(TypeIndex<T>());
        if (it == entity_to_component_.end()) return nullptr;

        const auto& mapping = it->second;
        auto mit = mapping.find(entity_id);
        if (mit == mapping.end()) return nullptr;

        auto& container = getContainer<T>();
        return &container[mit->second];
    }
    //エンティティがそのコンポーネントを所有しているかの確認
    template<typename T>
    inline bool Has(uint32_t entity_id)
    {
        auto it = entity_to_component_.find(TypeIndex<T>());
        if (it == entity_to_component_.end()) return false;
        return it->second.find(entity_id) != it->second.end();
    }

    //特定のコンポーネントを持つエンティティに対して一括処理をする為の走査関数
    //単数用
    template<typename T, typename Func>
    void ForEach(Func&& func)
    {

        const auto& type = TypeIndex<T>();
        auto entity_it = component_to_entity_.find(type);
        //コンポーネントが存在しない場合は何もしない
        if (  entity_it == component_to_entity_.end())
        {
            return;
        }

        auto& container = getContainer<T>();
        auto& entities = entity_it->second;

        const size_t count = container.size();

        //念のため、動機ミスの検出を行う
        assert(entities.size() == count);

        for (size_t i = 0; i < count; ++i)
        {
            func(entities[i], container[i]);
        }
    }
    //複数コンポーネント用
    template<typename First, typename Second, typename... Rest, typename Func>
    void ForEach(Func&& func)
    {
        auto& first_container = getContainer<First>();

        const auto& type = TypeIndex<First>();
        auto entity_it = component_to_entity_.find(type);

        if (entity_it == component_to_entity_.end())
        {
            return;
        }

        const auto& entities = entity_it->second;

        //念のため、動機ミスの検出を行う
        size_t count = entities.size();
        assert(count == first_container.size());


        for (size_t i = 0; i < count; ++i)
        {
            auto* second = TryGetByEntity<Second>(entities[i]);

            if (!second)
            {
                continue;
            }

            if constexpr (sizeof...(Rest) == 0)
            {
                func(entities[i], first_container[i], *second);
            }
            else
            {
                auto tuple = std::make_tuple(TryGetByEntity<Rest>(entities[i])...);

                bool all_exists = true;

                std::apply(
                    [&](auto*... ptrs)
                    {
                        all_exists = ((ptrs != nullptr) && ...);
                    },
                    tuple
                );

                if (!all_exists)
                {
                    continue;
                }

                std::apply(
                    [&](auto*... ptrs)
                    {
                        func(entities[i], first_container[i], *second, *ptrs...);
                    },
                    tuple
                );
            }
        }
    }

    //コンポーネントの持ち主のエンティティが死んだとき、属するコンポーネントを消す
    void RemoveAllComponents(uint32_t entity_id) {
        for (auto& [type, remove_fn] : removers_)
        {
            remove_fn(entity_id);
        }

    }

private:
    // 型ごとのコンテナを汎用的に扱うための仕組み
    template<typename T>
    void registerContainer(std::vector<T>& vec) {
        const auto& type = TypeIndex<T>();

        containers_[type] = &vec;
        component_to_entity_[type] = {};

        //removerも登録
        removers_[type] = [this](uint32_t eid)
            {
                this->Remove<T>(eid);
            };
    }

    template<typename T>
    std::vector<T>& getContainer() {
        const auto& type = TypeIndex<T>();
        auto it = containers_.find(type);
        if (it == containers_.end()) {
            throw std::runtime_error("�R���|�[�l���g���o�^����Ă��܂���");
        }
        return *static_cast<std::vector<T>*>(it->second);
    }

    template<typename T>
    const std::vector<T>& getContainer() const {
        auto it = containers_.find(TypeIndex<T>());
        if (it == containers_.end()) {
            throw std::runtime_error("�R���|�[�l���g���o�^����Ă��܂���");
        }
        return *static_cast<const std::vector<T>*>(it->second);
    }

private:
    std::unordered_map<std::type_index, void*> containers_;
    //型ごとのマッピング機能

    std::unordered_map<std::type_index, std::vector<uint32_t>>component_to_entity_;
    std::unordered_map<std::type_index, std::unordered_map<uint32_t, int>> entity_to_component_;
    std::unordered_map<std::type_index, std::function<void(uint32_t)>> removers_;

    std::vector<ComponentPosition> positions_;
    std::vector<ComponentRotation> rotations_;
    std::vector<ComponentScale> scales_;
    std::vector<ComponentLocalToWorld> l2ws_;
    std::vector<ComponentColor> colors_;
    std::vector<ComponentGltf> gltfs_;
    std::vector<ComponentMesh>meshes_;
    std::vector<ComponentPrimitive>primitives_;
    std::vector<ComponentMaterial>materials_;
    std::vector<ComponentTexture>textures_;
    std::vector<ComponentInstanced>instanced_;
    std::vector<ComponentSkyAtmosphere>skys_;
    std::vector<ComponentVolumetricCloud>clouds_;
    std::vector<ComponentAdjastPbrParamter>ajast_pbr_paramters_;
    std::vector<ComponentCamera>cameras_;
    std::vector<ComponentSsr>ssrs_;
    std::vector<ComponentName>names_;
    std::vector<ComponentCascadeShadow>cas_shadows_;
    std::vector<ComponentDeferredRender>deferred_renders_;
    std::vector<ComponentBoundingBox>bounding_boxes_;
};
