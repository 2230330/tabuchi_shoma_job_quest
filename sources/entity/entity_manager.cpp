#include"../../headers/entity/entity_manager.h"

//エンティティの追加
uint32_t EntityManager::Add() {
    uint32_t id;
    if (!free_list_.empty()) {
        id = free_list_.back();
        free_list_.pop_back();
        entities_[id].alive = true;
    }
    else {
        id = count_++;
        entities_.emplace_back(Entity{ id, true });
    }
    return id;
}

uint32_t EntityManager::AddWithID(uint32_t id)
{
    if (id >= entities_.size())
        entities_.resize(id + 1);

    entities_[id].entity = id;
    entities_[id].alive = true;

    count_ = std::max(count_, id + 1);

    return id;
}

//えんてぃてぃの削除
void EntityManager::Remove(uint32_t id) {
    if (id < entities_.size()) {
        entities_[id].alive = false;
        free_list_.push_back(id);
    }
}

const std::vector<Entity>& EntityManager::GetArray() const 
{
    return entities_;
}

//EntityRecordへの登録
void EntityManager::RegisterEntity(uint32_t entity_id, Archetype* archetype, uint32_t index_in_chunk)
{
    entity_records_[entity_id] = EntityRecord{ archetype,index_in_chunk };
}

//EntityRecordのゲッター
EntityRecord EntityManager::GetRecord(uint32_t entity_id) const
{
    auto it = entity_records_.find(entity_id);
    if (it != entity_records_.end())
    {
        return it->second;
    }

    //見つからなかった場合、デフォルト値(nullptr,無効インデックス)を返す
    return EntityRecord{ nullptr,static_cast<uint32_t>(-1) };
    return EntityRecord();
}
