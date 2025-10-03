#pragma once
#include"../archetype/archetype_manager.h"
#include"../entity/entity_manager.h"

class World
{
public:
    World() :archetype_manager_(&entity_manager_)
    {
    };

private:
    EntityManager entity_manager_;
    ArchetypeManager archetype_manager_;
};