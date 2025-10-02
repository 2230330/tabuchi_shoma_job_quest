#pragma once
#include"i_component.h"
#include<DirectXMath.h>

struct ComponentColor :public IComponent
{
    DirectX::XMFLOAT4 value;
};