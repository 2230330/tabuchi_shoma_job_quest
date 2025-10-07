#pragma once
#include"i_component.h"

struct ComponentTexture :public IComponent
{
    //テクスチャ自体を保持するのではなく、
    //テクスチャ配列のインデックスを保持する
    int texture_id;
};
