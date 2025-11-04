#pragma once
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <stdexcept>

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
#include "component_sky_atmosphere.h"
#include "component_cloud_dome.h"

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
        registerContainer<ComponentCloudDome>(clouds_);
    }

    template<typename T>
    int Add(const T& component) {
        auto& container = getContainer<T>();
        container.push_back(component);
        return static_cast<int>(container.size() - 1);
    }

    template<typename T>
    int Add(uint16_t entity_id, const T& component) {
        auto& container = getContainer<T>();
        container.emplace_back(component);
        int id = static_cast<int>(container.size() - 1);
        entity_to_component_[std::type_index(typeid(T))][entity_id] = id;
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
        auto type = std::type_index(typeid(T));
        auto mit = entity_to_component_.find(type);
        if (mit == entity_to_component_.end()) return;

        auto& mapping = mit->second;
        auto it = mapping.find(entity_id);
        if (it == mapping.end()) return;

        int index_to_remove = it->second;
        auto& container = getContainer<T>();
        int last_index = static_cast<int>(container.size() - 1);

        if (index_to_remove != last_index) {
            // swap with last
            std::swap(container[index_to_remove], container[last_index]);

            // 更新対象の entity を探す
            for (auto& [eid, idx] : mapping) {
                if (idx == last_index) {
                    idx = index_to_remove;
                    break;
                }
            }
        }

        container.pop_back();
        mapping.erase(entity_id);
    }

    //一緒に登録したエンティティで要素を取り出すゲッター
    template<typename T>
    T& GetByEntity(uint32_t entity_id) {
        auto& container = getContainer<T>();
        auto& mapping = entity_to_component_[std::type_index(typeid(T))];
        return container.at(mapping.at(entity_id));
    }

    //登録したコンポーネントがない場合などの安全版ゲッター
    template<typename T>
    T* TryGetByEntity(uint32_t entity_id) {
        auto it = entity_to_component_.find(std::type_index(typeid(T)));
        if (it == entity_to_component_.end()) return nullptr;

        auto& mapping = it->second;
        auto mit = mapping.find(entity_id);
        if (mit == mapping.end()) return nullptr;

        auto& container = getContainer<T>();
        return &container.at(mit->second);
    }
    //エンティティがそのコンポーネントを所有しているかの確認
    template<typename T>
    inline bool Has(uint32_t entity_id) 
    {
        auto it = entity_to_component_.find(std::type_index(typeid(T)));
        if (it == entity_to_component_.end()) return false;
        return it->second.find(entity_id) != it->second.end();
    }

    //特定のコンポーネントを持つエンティティに対して一括処理をする為の走査関数
    template<typename T>
    void ForEach(std::function<void(uint32_t, T&)> func) {
        auto& container = getContainer<T>();
        auto& mapping = entity_to_component_[std::type_index(typeid(T))];
        for (const auto& [entity_id, index] : mapping) {
            func(entity_id, container[index]);
        }
    }

    //コンポーネントの持ち主のエンティティが死んだとき、属するコンポーネントを消す
    void RemoveAllComponents(uint32_t entity_id) {
        for (auto& [type, mapping] : entity_to_component_) {
            mapping.erase(entity_id);
        }
    }

private:
    // 型ごとのコンテナを汎用的に扱うための仕組み
    template<typename T>
    void registerContainer(std::vector<T>& vec) {
        containers_[std::type_index(typeid(T))] = &vec;
    }

    template<typename T>
    std::vector<T>& getContainer() {
        auto it = containers_.find(std::type_index(typeid(T)));
        if (it == containers_.end()) {
            throw std::runtime_error("コンポーネントが登録されていません");
        }
        return *static_cast<std::vector<T>*>(it->second);
    }

    template<typename T>
    const std::vector<T>& getContainer() const {
        auto it = containers_.find(std::type_index(typeid(T)));
        if (it == containers_.end()) {
            throw std::runtime_error("コンポーネントが登録されていません");
        }
        return *static_cast<const std::vector<T>*>(it->second);
    }

private:
    std::unordered_map<std::type_index, void*> containers_;
    //型ごとのマッピング機能
    std::unordered_map<std::type_index, std::unordered_map<uint32_t, int>> entity_to_component_;

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
    std::vector<ComponentCloudDome>clouds_;
};
