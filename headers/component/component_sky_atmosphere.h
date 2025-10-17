#pragma once
#include"i_component.h"

//これがついてるコンポーネントを天球に使うタグコンポーネント
struct ComponentSkyAtmosphere :public IComponent
{
    bool tag = true;
};