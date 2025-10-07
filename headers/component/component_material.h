#pragma once
#include"i_component.h"

struct ComponentMaterial :public IComponent
{
    //マテリアル自体を保持するのではなく、
    //マテリアルが格納されている配列のどこにあるのかを示す
    int material_id;
};