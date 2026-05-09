#pragma once
#include<d3d11.h>
#include<wrl.h>
#include<array>

#include"i_component.h"
#include"../system/render_deferred_system.h"

struct ComponentCascadeShadow :public IComponent
{
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 3>srvs_;
};