#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <typeindex>
#include <set>
#include<functional>
#include "archetype.h"
#include"../entity/entity_manager.h"

//マップの為にカスタムハッシュを実装します
struct TypeSetHash
{
    size_t operator()(const std::set<std::type_index>& types)const
    {
        size_t seed = 0;
        for (const auto& type : types) {
            seed ^= type.hash_code() + 0x9e3779b9/*黄金比の定数、許して*/ + (seed << 6) + (seed >> 2);
        }
        return seed;

    }
};

/// <summary>
/// エンティティの管理者
/// これがあると、型セットで走査対象を絞ることが出来る。
/// </summary>
class ArchetypeManager {
public:
    ArchetypeManager(EntityManager*entity_mng);

    // コンポーネント型のセットからアーキタイプを取得
    Archetype* GetOrCreateArchetype(const std::set<std::type_index>& component_types);

    // Entity をアーキタイプに追加
    void AddEntity(uint32_t entity_id, const std::set<std::type_index>& component_types);

    // Archetype走査用
    const std::vector<std::unique_ptr<Archetype>>& GetAllArchetypes() const { return archetypes_; }

private:
    //コンポーネントの集合をキーに、対応するアーチタイプへのポインタを持つマップ
    std::unordered_map<std::set<std::type_index>, Archetype*, TypeSetHash> archetype_map_;
    //実際のアーチタイプを所有するコンテナ
    std::vector<std::unique_ptr<Archetype>> archetypes_;
    //エンティティマネージャを持つが、構造上コピー不可になる可能性が高いので、ポインタで管理します
    EntityManager* entity_mng_;
};