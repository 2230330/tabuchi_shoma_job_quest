#include"../../headers/system/update_animation_system.h"

#include"../../headers/component/component_manager.h"

UpdateAnimationSystem::UpdateAnimationSystem(ComponentManager& comp_mng)
    : comp_mng_(comp_mng)
{
}

void UpdateAnimationSystem::Update(float elapsed_time)
{
    comp_mng_.ParallelForEach<
        ComponentAnimation,
        ComponentGltf,
        ComponentPosition
    >(
        [&](uint32_t entity_id,
            ComponentAnimation,
            ComponentGltf& gltf,
            ComponentPosition& position)
        {
            if (!gltf.model)
                return;

            const auto& animations = gltf.model->GetAnimations();

            if (animations.size() <= gltf.animation_index)
                return;

            const float duration = animations[gltf.animation_index].duration;

            if (duration <= 0.0f)
                return;

            gltf.animation_time += elapsed_time * gltf.animation_speed;

            if (gltf.animation_time > duration)
            {
                if (gltf.loop)
                {
                    gltf.animation_time = fmod(gltf.animation_time, duration);
                }
                else
                {
                    gltf.animation_time = duration;
                }
            }

           

            gltf.model->UpdateAnimation(
                gltf.animation_index,
                gltf.animation_time,
                gltf.animated_nodes);
        },
        1024
    );
}