#pragma once
#include<vector>

#include"entity.h"
#include"entity_record.h"

class EntityManager {
public:
    //エンティティの追加関数
    //追加されたエンティティは、フリーリストに開いている番号がある場合、そこに入る
    uint32_t Add();

    //エンティティの割り込み
    uint32_t AddWithID(uint32_t id);

    //エンティティの削除関数
    //削除されたエンティティ番号はフリーリストに入り、再利用される
    void Remove(uint32_t id);

    //配列の呼び出し。
    const std::vector<Entity>& GetArray() const;

    //EntityRecordへの登録
    void RegisterEntity(uint32_t entity_id, Archetype* archetype, uint32_t index_in_chunk);
    //エンティティレコードのゲッター
    EntityRecord GetRecord(uint32_t entity_id)const;


private:
    uint32_t count_ = 0;
    std::vector<Entity> entities_;
    std::vector<uint32_t> free_list_;
    //エンティティがどのアーキタイプに属しているか
    std::unordered_map<uint32_t, EntityRecord>entity_records_;
};