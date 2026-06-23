#pragma once
#include"i_component.h"
#include<DirectXMath.h>

struct ComponentPosition :public IComponent
{
    DirectX::XMFLOAT3 value{ 0.f,0.f,0.f };
};
