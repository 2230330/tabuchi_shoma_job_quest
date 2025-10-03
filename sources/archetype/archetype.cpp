#include"../../headers/archetype/archetype.h"

Archetype::Archetype(const std::set<std::type_index>& types)
    :types_(types)
{
}

//アーチタイプに新しいIDを登録し、チャンクのどこにいるのかを返す。
uint32_t Archetype::AddEntity(uint32_t entity_id) {
    entities_.push_back(entity_id);
    return static_cast<uint32_t>(entities_.size() - 1); // index_in_chunk を返す
}


std::vector<uint32_t>& Archetype::GetEntityList()
{
    return entities_;
}