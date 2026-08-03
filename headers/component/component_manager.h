#pragma once

#include <vector>
#include <cstdint>
#include <cassert>
#include <tuple>
#include <algorithm>
#include <utility>

#include"component_storage.h"

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
#include "component_ajast_pbr_paramter_.h"
#include "component_camera.h"
#include "component_screen_space_reflection.h"
#include "component_name.h"
#include "component_cascade_shadow.h"
#include "component_deferred_render.h"
#include "component_bound_box.h"
#include "component_dynamic.h"
#include "component_fog.h"
#include "component_golden_spiral.h"

#include "../job_system.h"


class ComponentManager
{
public:
    ComponentManager() = default;

    //型Tに対するComponentStorageを取得する
    //もともと、std::type_indexとunordered_mapを使用してコンポーネントの配列を検索していましたが、
    //膨大な数のコンポーネントをいちいちマップで検索していては、どうしても処理が重くなるので変更しました
    // 
    // この実装では、Component型ごとに専用のストレージメンバを持ち、
    // Storage<T>によって、Tに対応するストレージを呼び出す
    // これの実装により、ホットパスを軽くできました。
    //
    template<typename T>
    ComponentStorage<T>& Storage();

    template<typename T>
    const ComponentStorage<T>& Storage() const;

    template<typename T>
    int Add(uint32_t entity_id, const T& component)
    {
        return Storage<T>().Add(entity_id, component);
    }

    template<typename T, typename... Args>
    int Emplace(uint32_t entity_id, Args&&... args)
    {
        return Storage<T>().Emplace(entity_id, std::forward<Args>(args)...);
    }

    template<typename T>
    void Remove(uint32_t entity_id)
    {
        Storage<T>().Remove(entity_id);
    }

    template<typename T>
    bool Has(uint32_t entity_id) const
    {
        return Storage<T>().Has(entity_id);
    }

    template<typename T>
    T* TryGetByEntity(uint32_t entity_id)
    {
        return Storage<T>().TryGet(entity_id);
    }

    template<typename T>
    const T* TryGetByEntity(uint32_t entity_id) const
    {
        return Storage<T>().TryGet(entity_id);
    }

    template<typename T>
    T& GetByEntity(uint32_t entity_id)
    {
        return Storage<T>().GetByEntity(entity_id);
    }

    template<typename T>
    const T& GetByEntity(uint32_t entity_id) const
    {
        return Storage<T>().GetByEntity(entity_id);
    }

    template<typename T>
    T& Get(int id)
    {
        return Storage<T>().GetByIndex(id);
    }

    template<typename T>
    const T& Get(int id) const
    {
        return Storage<T>().GetByIndex(id);
    }

    template<typename T>
    void Reserve(size_t component_count, size_t entity_capacity)
    {
        Storage<T>().Reserve(component_count, entity_capacity);
    }

    //単数用
    template<typename T, typename Func>
    void ForEach(Func&& func)
    {
        auto& storage = Storage<T>();

        const size_t count = storage.components.size();

        assert(storage.entities.size() == count);

        for (size_t i = 0; i < count; ++i)
        {
            func(storage.entities[i], storage.components[i]);
        }
    }

    //複数用
    template<typename First, typename Second, typename... Rest, typename Func>
    void ForEach(Func&& func)
    {
        auto& first_storage = Storage<First>();

        const size_t count = first_storage.components.size();

        assert(first_storage.entities.size() == count);

        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t entity_id = first_storage.entities[i];

            auto second = TryGetByEntity<Second>(entity_id);

            if (second == nullptr)
            {
                continue;
            }

            if constexpr (sizeof...(Rest) == 0)
            {
                func(entity_id, first_storage.components[i], *second);
            }
            else
            {
                auto rest_tuple = std::make_tuple(TryGetByEntity<Rest>(entity_id)...);

                bool all_exists = true;

                std::apply(
                    [&](auto... ptrs)
                    {
                        all_exists = ((ptrs != nullptr) && ...);
                    },
                    rest_tuple
                );

                if (!all_exists)
                {
                    continue;
                }

                std::apply(
                    [&](auto... ptrs)
                    {
                        func(entity_id, first_storage.components[i], *second, *ptrs...);
                    },
                    rest_tuple
                );
            }
        }
    }

    template<typename First, typename Second, typename... Rest, typename Func>
    void ParallelForEach(Func&& func, size_t batch_size = 128)
    {
        auto& first_storage = Storage<First>();

        const size_t count = first_storage.components.size();

        assert(first_storage.entities.size() == count);

        JobSystem::Instance().ParallelFor(
            count,
            batch_size,
            [&](size_t i)
            {
                const uint32_t entity_id = first_storage.entities[i];

                auto second = TryGetByEntity<Second>(entity_id);

                if (second == nullptr)
                {
                    return;
                }

                if constexpr (sizeof...(Rest) == 0)
                {
                    func(entity_id, first_storage.components[i], *second);
                }
                else
                {
                    auto rest_tuple = std::make_tuple(TryGetByEntity<Rest>(entity_id)...);

                    bool all_exists = true;

                    std::apply(
                        [&](auto... ptrs)
                        {
                            all_exists = ((ptrs != nullptr) && ...);
                        },
                        rest_tuple
                    );

                    if (!all_exists)
                    {
                        return;
                    }

                    std::apply(
                        [&](auto... ptrs)
                        {
                            func(entity_id, first_storage.components[i], *second, *ptrs...);
                        },
                        rest_tuple
                    );
                }
            }
        );
    }

    void RemoveAllComponents(uint32_t entity_id)
    {
        Remove<ComponentPosition>(entity_id);
        Remove<ComponentRotation>(entity_id);
        Remove<ComponentScale>(entity_id);
        Remove<ComponentLocalToWorld>(entity_id);
        Remove<ComponentColor>(entity_id);
        Remove<ComponentGltf>(entity_id);
        Remove<ComponentMesh>(entity_id);
        Remove<ComponentPrimitive>(entity_id);
        Remove<ComponentMaterial>(entity_id);
        Remove<ComponentTexture>(entity_id);
        Remove<ComponentInstanced>(entity_id);
        Remove<ComponentSkyAtmosphere>(entity_id);
        Remove<ComponentVolumetricCloud>(entity_id);
        Remove<ComponentAdjastPbrParamter>(entity_id);
        Remove<ComponentCamera>(entity_id);
        Remove<ComponentSsr>(entity_id);
        Remove<ComponentName>(entity_id);
        Remove<ComponentCascadeShadow>(entity_id);
        Remove<ComponentDeferredRender>(entity_id);
        Remove<ComponentBoundingBox>(entity_id);
        Remove<ComponentDynamic>(entity_id);
        Remove<ComponentFog>(entity_id);
        Remove<ComponentGoldenSpiral>(entity_id);
    }

    void ClearAll()
    {
        positions_.Clear();
        rotations_.Clear();
        scales_.Clear();
        l2ws_.Clear();
        colors_.Clear();
        gltfs_.Clear();
        meshes_.Clear();
        primitives_.Clear();
        materials_.Clear();
        textures_.Clear();
        instanced_.Clear();
        skys_.Clear();
        clouds_.Clear();
        ajast_pbr_paramters_.Clear();
        cameras_.Clear();
        ssrs_.Clear();
        names_.Clear();
        cas_shadows_.Clear();
        deferred_renders_.Clear();
        bounding_boxes_.Clear();
        dynamics_.Clear();
        fogs_.Clear();
        golden_spiral_.Clear();
    }

private:
    ComponentStorage<ComponentPosition> positions_;
    ComponentStorage<ComponentRotation> rotations_;
    ComponentStorage<ComponentScale> scales_;
    ComponentStorage<ComponentLocalToWorld> l2ws_;
    ComponentStorage<ComponentColor> colors_;
    ComponentStorage<ComponentGltf> gltfs_;
    ComponentStorage<ComponentMesh> meshes_;
    ComponentStorage<ComponentPrimitive> primitives_;
    ComponentStorage<ComponentMaterial> materials_;
    ComponentStorage<ComponentTexture> textures_;
    ComponentStorage<ComponentInstanced> instanced_;
    ComponentStorage<ComponentSkyAtmosphere> skys_;
    ComponentStorage<ComponentVolumetricCloud> clouds_;
    ComponentStorage<ComponentAdjastPbrParamter> ajast_pbr_paramters_;
    ComponentStorage<ComponentCamera> cameras_;
    ComponentStorage<ComponentSsr> ssrs_;
    ComponentStorage<ComponentName> names_;
    ComponentStorage<ComponentCascadeShadow> cas_shadows_;
    ComponentStorage<ComponentDeferredRender> deferred_renders_;
    ComponentStorage<ComponentBoundingBox> bounding_boxes_;
    ComponentStorage<ComponentDynamic> dynamics_;
    ComponentStorage<ComponentFog> fogs_;
    ComponentStorage<ComponentGoldenSpiral>golden_spiral_;
};


//コンポーネント型とその型を格納しているComponentStorageメンバの対応を定義します
// 
// 新しいコンポーネントを追加した場合は、
// 対応する型のコンポーネントストレージと
// 対応する型のマクロを生成する
//
#define DEFINE_COMPONENT_STORAGE(Type, Member)                                  \
template<>                                                                      \
inline ComponentStorage<Type>& ComponentManager::Storage<Type>()                 \
{                                                                                \
    return Member;                                                               \
}                                                                                \
template<>                                                                      \
inline const ComponentStorage<Type>& ComponentManager::Storage<Type>() const      \
{                                                                                \
    return Member;                                                               \
}

DEFINE_COMPONENT_STORAGE(ComponentPosition, positions_)
DEFINE_COMPONENT_STORAGE(ComponentRotation, rotations_)
DEFINE_COMPONENT_STORAGE(ComponentScale, scales_)
DEFINE_COMPONENT_STORAGE(ComponentLocalToWorld, l2ws_)
DEFINE_COMPONENT_STORAGE(ComponentColor, colors_)
DEFINE_COMPONENT_STORAGE(ComponentGltf, gltfs_)
DEFINE_COMPONENT_STORAGE(ComponentMesh, meshes_)
DEFINE_COMPONENT_STORAGE(ComponentPrimitive, primitives_)
DEFINE_COMPONENT_STORAGE(ComponentMaterial, materials_)
DEFINE_COMPONENT_STORAGE(ComponentTexture, textures_)
DEFINE_COMPONENT_STORAGE(ComponentInstanced, instanced_)
DEFINE_COMPONENT_STORAGE(ComponentSkyAtmosphere, skys_)
DEFINE_COMPONENT_STORAGE(ComponentVolumetricCloud, clouds_)
DEFINE_COMPONENT_STORAGE(ComponentAdjastPbrParamter, ajast_pbr_paramters_)
DEFINE_COMPONENT_STORAGE(ComponentCamera, cameras_)
DEFINE_COMPONENT_STORAGE(ComponentSsr, ssrs_)
DEFINE_COMPONENT_STORAGE(ComponentName, names_)
DEFINE_COMPONENT_STORAGE(ComponentCascadeShadow, cas_shadows_)
DEFINE_COMPONENT_STORAGE(ComponentDeferredRender, deferred_renders_)
DEFINE_COMPONENT_STORAGE(ComponentBoundingBox, bounding_boxes_)
DEFINE_COMPONENT_STORAGE(ComponentDynamic, dynamics_)
DEFINE_COMPONENT_STORAGE(ComponentFog, fogs_)
DEFINE_COMPONENT_STORAGE(ComponentGoldenSpiral, golden_spiral_)

#undef DEFINE_COMPONENT_STORAGE