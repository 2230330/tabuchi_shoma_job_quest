#pragma once
#include"i_component.h"
#include<d3d11.h>
#include<wrl.h>
#include<vector>

struct ComponentDeferredRender :public IComponent
{
    std::vector<ID3D11ShaderResourceView*>srvs_;
};