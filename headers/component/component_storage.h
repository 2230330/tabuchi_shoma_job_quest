#pragma once

#include<vector>
#include <cstdint>
#include<cassert>
#include<cstddef>
#include<algorithm>
#include<utility>


//一種類のコンポーネントを取り扱うためのストレージ
//コンポーネントをEntityIDと対応付けながら高速に管理するための入れ物
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

    //entity_to_indexがentity_idを扱えるサイズになるように拡張する
    //拡張された部分は-1で初期化する
    void EnsureEntityCapacity(uint32_t entity_id)
    {
        if (entity_id >= entity_to_index.size())
        {
            entity_to_index.resize(static_cast<size_t>(entity_id) + 1, -1);
        }
    }

    //あらかじめメモリを確保しておく。
    //component_count:このStorageに入るコンポーネント数の予想値
    //entity_capacity:扱う可能性のあるEntityIDの最大数に近い値
    //reserveしておくことで、Add時のvector再確保を減らせる
    //再確保はコストがかかる上、コンポーネント配列内の要素のアドレスも変わるので、できるだけ減らしたい
    void Reserve(size_t component_count, size_t entity_capacity)
    {
        components.reserve(component_count);
        entities.reserve(component_count);

        if (entity_to_index.size() < entity_capacity)
        {
            entity_to_index.resize(entity_capacity, -1);
        }
    }

    //指定したEntityが、このコンポーネント型を持っているか調べる
    //entity_to_index[entity_id]が0以上なら、
    // そのEntityはこのコンポーネントを持っている
    //
    bool Has(uint32_t entity_id) const
    {
        return entity_id < entity_to_index.size()
            && entity_to_index[entity_id] >= 0;
    }

    //指定したEntityのコンポーネントを取得する
    //持ってないなら、nullptrを渡す
    //GetByEntityと違い、存在しない場合でも落ちないので、
    //あるかどうかわからないComponentを取る時に使います
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
    //固定のTryGet
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

    //
    // 指定したEntityのComponentを取得する
    // 必ず持っている前提で使うこと
    //
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

    //
    // componentsのIndexを直接指定して、コンポーネントを取得する
    // entity_idではなく、Storage内部のIndexでアクセスする
    // at()を使っているので、範囲外の値を入れると例外が発生する
    //
    T& GetByIndex(int index)
    {
        return components.at(static_cast<size_t>(index));
    }
    const T& GetByIndex(int index) const
    {
        return components.at(static_cast<size_t>(index));
    }

    //
    // 指定したEntityにComponentを追加する
    // 追加したcomponentはcomponentsの末尾に入る
    // entitiesの末尾にはentity_idが入る
    // entity_to_index[entity_id]には、追加されたコンポーネントのindexが入る
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



    // コンポーネントを直接構築して追加する。
    // Add は完成済みの コンポーネントを受け取る。
    // Emplace は constructor の引数を受け取り、components 内で直接 T を構築する。
    // 今の コンポーネント が単純な構造体中心なら Add だけでも十分だが、
    // 将来的に初期化コストのある コンポーネント が増えた場合に便利と考えます。
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

    //指定したEntityのコンポーネントを削除する
    //ストレージは削除を高速にするため、swap-removeを使います
    //この方法では、削除したい要素を、配列の最後の要素で上書きします。
    //その後、末尾をpop-backします。途中の要素を詰める必要がないため処理が速いです。
    //デメリットとして、要素の並び順が維持されません
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

    //ストレージ内のコンポーネントを削除します
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
