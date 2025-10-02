#pragma once
#include"i_component.h"
#include<DirectXMath.h>

struct ComponentScale :public IComponent
{
    DirectX::XMFLOAT3 value;
};