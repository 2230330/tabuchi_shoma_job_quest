#pragma once
#include"i_component.h"
#include<DirectXMath.h>

//簡易的なフォグ
struct ComponentFog :public IComponent
{
    int fog_steps{ 32 };
    float fog_max_distance{ 1000.f };
    float noise_scale{ 0.003f };
    float fog_density{ 0.001f };

    float fog_height_max{ 100.f };
    float fog_intensity{ 1.f };

    DirectX::XMFLOAT4 fog_color{ 0.5f,0.5f,0.5f,1.0f };
    int use_noise{ 1 };

    float gpu_time_ms= 0.0f;
};
