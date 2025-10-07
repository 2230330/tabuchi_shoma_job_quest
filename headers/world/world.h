#pragma once
#include"../archetype/archetype_manager.h"
#include"../entity/entity_manager.h"

class World
{
private:
    EntityManager entity_manager_;
    ArchetypeManager archetype_manager_;

public:
    World() :archetype_manager_(&entity_manager_)
    {
    }

    EntityManager* GetEntityManager() { return &entity_manager_; }
    ArchetypeManager* GetArcheTypeManager() { return &archetype_manager_; }

};