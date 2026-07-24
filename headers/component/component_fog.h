#pragma once
#include"i_component.h"

//簡易的なフォグ
struct ComponentFog :public IComponent
{
    int fog_steps{ 32 };
    float fog_max_distance{ 300.f };
    float noise_scale{ 0.015f };
    float fog_density{ 0.005f };
    float fog_height_max{ 100.f };

    float gpu_time_ms= 0.0f;
};
