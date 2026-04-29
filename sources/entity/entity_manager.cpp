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

//uint32_t EntityManager::AddWithID(uint32_t id)
//{
//    if (id >= entities_.size())
//        entities_.resize(id + 1);
//
//    entities_[id].entity = id;
//    entities_[id].alive = true;
//
//    count_ = std::max(count_, id + 1);
//
//    return id;
//}

bool EntityManager::AddWithID(uint32_t id)
{
    //必要なら拡張子、新規領域を安全に初期化
    if (id >= entities_.size())
    {
        const size_t old_size = entities_.size();
        entities_.resize(id + 1);
        for (size_t i = old_size; i < entities_.size(); ++i)
        {
            entities_[i].entity = -1;
            entities_[i].alive = false;
        }
    }

    //free_list_にIDがあれば取り除く
    if (!free_list_.empty())
    {
        auto it = std::find(free_list_.begin(), free_list_.end(), id);
        if (it != free_list_.end())
        {
            free_list_.erase(it);
        }
    }

    //既に生存していたら上書き不可(ロード前に全削除していればヒットしないはず)
    if (entities_[id].alive && entities_[id].entity != -1&& entities_[id].entity != id)
    {
        return false;//不整合
    }

    //生成
    entities_[id].entity = id;
    entities_[id].alive = true;

    //count_を更新
    if (count_ < id + 1)
    {
        count_ = id + 1;
    }

    return true;
}

//えんてぃてぃの削除

void EntityManager::Remove(uint32_t id) {
    if (id < entities_.size() && entities_[id].alive) {
        entities_[id].alive = false;
        entity_records_.erase(id);           // records の整合も保つ
        free_list_.push_back(id);            // alive==true の時だけ push（重複防止）
    }
}


const std::vector<Entity>& EntityManager::GetArray() const 
{
    return entities_;
}

//EntityRecordへの登録
void EntityManager::RegisterEntity(uint32_t entity_id, uint32_t index_in_chunk)
{
    entity_records_[entity_id] = EntityRecord{ index_in_chunk };
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
    return EntityRecord{ static_cast<uint32_t>(-1) };
    return EntityRecord();
}
