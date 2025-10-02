#pragma once
#include<vector>

#include"entity.h"
class EntityManager {
public:
    //エンティティの追加関数
    //追加されたエンティティは、フリーリストに開いている番号がある場合、そこに入る
    uint32_t Add();

    //エンティティの削除関数
    //削除されたエンティティ番号はフリーリストに入り、再利用される
    void Remove(uint32_t id);

    //配列の呼び出し。
    const std::vector<Entity>& GetArray() const;

private:
    uint32_t count_ = 0;
    std::vector<Entity> entities_;
    std::vector<uint32_t> free_list_;
};