#pragma once
#include"i_component.h"
#include<DirectXMath.h>

//バウンディングボックスコンポーネント
struct ComponentBoundingBox :public IComponent
{
    DirectX::XMFLOAT3 local_min{ +FLT_MAX, +FLT_MAX, +FLT_MAX };
    DirectX::XMFLOAT3 local_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

    DirectX::XMFLOAT3 world_min{ +FLT_MAX, +FLT_MAX, +FLT_MAX };
    DirectX::XMFLOAT3 world_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
};