#include"../../headers/world/world.h"

#include"../../headers/entity/entity_manager.h"

World::World()
{
    entity_manager_ = std::make_unique<EntityManager>();
}

World::~World() = default;

EntityManager* World::GetEntityManager()
{
    return entity_manager_.get();
}