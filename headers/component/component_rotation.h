#pragma once
#include"i_component.h"
#include<DirectXMath.h>

struct ComponentRotation :public IComponent
{
    DirectX::XMFLOAT3 value;
};