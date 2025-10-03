#include"../../headers/archetype/archetype_manager.h"

ArchetypeManager::ArchetypeManager(EntityManager* entity_mng)
    :entity_mng_(entity_mng)
{
}

//入れられたコンポーネントのタイプからアーチタイプを取得
Archetype* ArchetypeManager::GetOrCreateArchetype(const std::set<std::type_index>& component_types)
{
    //前からあるかどうかの確認
    auto it = archetype_map_.find(component_types);
    if (it != archetype_map_.end())
    {
        return it->second;
    }

    //新規登録
    auto new_archetype = std::make_unique<Archetype>(component_types);
    Archetype* ptr = new_archetype.get();
    archetypes_.emplace_back(std::move(new_archetype));
    archetype_map_[component_types] = ptr;

    return ptr;
}

void ArchetypeManager::AddEntity(uint32_t entity_id, const std::set<std::type_index>& component_types)
{
    //対応するアーキタイプを取得、無ければ生成
    Archetype* archetype = GetOrCreateArchetype(component_types);

    //チャンクにエンティティを追加し、インデックスを取得
    uint32_t index_in_chunk = archetype->AddEntity(entity_id);

    //EntityManagerに所属情報を追加
    entity_mng_->RegisterEntity(entity_id, archetype, index_in_chunk);
}
