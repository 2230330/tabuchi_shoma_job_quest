#pragma once
#include"i_component.h"
#include<DirectXMath.h>

struct ComponentLocalToWorld:public IComponent
{
    DirectX::XMFLOAT4X4 value;
};