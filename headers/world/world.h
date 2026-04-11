#pragma once
#include"../entity/entity_manager.h"

/// ゲーム全体の状態を管理するクラス
//現在はエンティティマネージャーのみを持つが、将来的にはシステムマネージャーやアーキタイプマネージャーも持つ予定
class World
{
private:
    EntityManager entity_manager_;

public:
    World() 
    {
    }

    EntityManager* GetEntityManager() { return &entity_manager_; }

};