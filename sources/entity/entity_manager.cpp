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
