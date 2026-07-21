#pragma once
#include"i_component.h"

//簡易的なフォグ
struct ComponentFog :public IComponent
{
    float fog_steps{ 32 };
    float fog_max_distance{ 300 };
    float noise_scale{ 0.015 };
    float fog_density{ 0.005 };

    float gpu_time_ms= 0.0f;
};
