#pragma once
#include<d3d11.h>
#include<wrl.h>
#include<array>

#include"i_component.h"
#include"../system/render_deferred_system.h"

struct ComponentCascadeShadow :public IComponent
{
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 3>srvs_;
    float intensity = 0.5f;
    float gpu_time_ms{ 0.0f };
};