#pragma once

#include<vector>
#include <cstdint>


//一種類のコンポーネントを取り扱うためのストレージ
template<typename T>
struct ComponentStorage
{
    // componentsに実際のコンポーネントを格納
    std::vector<T> components;
    // entitiesはcomponentsの要素がどのEntityに属しているかを表す配列
    //必ずcomponents と対応させる
    std::vector<uint32_t> entities;

    //entity_idからcomponentsのIndexを引くための表
    // entity_idがそのコンポーネントを 持っていない場合は -1
    std::vector<int> entity_to_index;

    void EnsureEntityCapacity(uint32_t entity_id)
    {
        if (entity_id >= entity_to_index.size())
        {
            entity_to_index.resize(static_cast<size_t>(entity_id) + 1, -1);
        }
    }

    void Reserve(size_t component_count, size_t entity_capacity)
    {
        components.reserve(component_count);
        entities.reserve(component_count);

        if (entity_to_index.size() < entity_capacity)
        {
            entity_to_index.resize(entity_capacity, -1);
        }
    }

    bool Has(uint32_t entity_id) const
    {
        return entity_id < entity_to_index.size()
            && entity_to_index[entity_id] >= 0;
    }

    T* TryGet(uint32_t entity_id)
    {
        if (entity_id >= entity_to_index.size())
        {
            return nullptr;
        }

        const int index = entity_to_index[entity_id];

        if (index < 0)
        {
            return nullptr;
        }

        return &components[static_cast<size_t>(index)];
    }

    const T* TryGet(uint32_t entity_id) const
    {
        if (entity_id >= entity_to_index.size())
        {
            return nullptr;
        }

        const int index = entity_to_index[entity_id];

        if (index < 0)
        {
            return nullptr;
        }

        return &components[static_cast<size_t>(index)];
    }

    T& GetByEntity(uint32_t entity_id)
    {
        assert(Has(entity_id));
        return components[static_cast<size_t>(entity_to_index[entity_id])];
    }

    const T& GetByEntity(uint32_t entity_id) const
    {
        assert(Has(entity_id));
        return components[static_cast<size_t>(entity_to_index[entity_id])];
    }

    T& GetByIndex(int index)
    {
        return components.at(static_cast<size_t>(index));
    }

    const T& GetByIndex(int index) const
    {
        return components.at(static_cast<size_t>(index));
    }

    int Add(uint32_t entity_id, const T& component)
    {
        EnsureEntityCapacity(entity_id);

        // 同じ Entity に同じ Component を二重追加しない
        assert(entity_to_index[entity_id] < 0);

        const int index = static_cast<int>(components.size());

        components.emplace_back(component);
        entities.emplace_back(entity_id);
        entity_to_index[entity_id] = index;

        return index;
    }

    template<typename... Args>
    int Emplace(uint32_t entity_id, Args&&... args)
    {
        EnsureEntityCapacity(entity_id);

        assert(entity_to_index[entity_id] < 0);

        const int index = static_cast<int>(components.size());

        components.emplace_back(std::forward<Args>(args)...);
        entities.emplace_back(entity_id);
        entity_to_index[entity_id] = index;

        return index;
    }

    void Remove(uint32_t entity_id)
    {
        if (entity_id >= entity_to_index.size())
        {
            return;
        }

        const int index = entity_to_index[entity_id];

        if (index < 0)
        {
            return;
        }

        const size_t remove_index = static_cast<size_t>(index);
        const size_t last_index = components.size() - 1;

        if (remove_index != last_index)
        {
            components[remove_index] = std::move(components[last_index]);

            const uint32_t moved_entity = entities[last_index];
            entities[remove_index] = moved_entity;

            entity_to_index[moved_entity] = static_cast<int>(remove_index);
        }

        components.pop_back();
        entities.pop_back();

        entity_to_index[entity_id] = -1;
    }

    void Clear()
    {
        components.clear();
        entities.clear();
        std::fill(entity_to_index.begin(), entity_to_index.end(), -1);
    }

    size_t Size() const
    {
        return components.size();
    }

    bool Empty() const
    {
        return components.empty();
    }
};
