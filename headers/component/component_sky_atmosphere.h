#pragma once
#include"i_component.h"

//これがついてるコンポーネントを天球に使うタグコンポーネント
struct ComponentSkyAtmosphere :public IComponent
{
    float rayleigh_scale_height{ 8000.f };
    float mie_scale_height{ 1200.f };
    float ozone_scale_half_width{ 15000.f };
    float ozone_center_height{ 50000.f };
    float earth_height{ 6360000.0f }; // 地球半径 [m]
    float sun_distance{ 150000000000.0f }; // 太陽までの距離 [m]
    float atmosphere_height{ 100000.0f }; // 大気の高さ [m]
    int max_sample{ 64 };
};