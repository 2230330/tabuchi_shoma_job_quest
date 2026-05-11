#pragma once

#include<memory>

//前方宣言
class EntityManager;


/// ゲーム全体の状態を管理するクラス
//現在はエンティティマネージャーのみを持つが、将来的にはシステムマネージャーやアーキタイプマネージャーも持つ予定
class World
{
public:
    World();
    ~World();

    EntityManager* GetEntityManager();
private:
    std::unique_ptr<EntityManager> entity_manager_=nullptr;


};