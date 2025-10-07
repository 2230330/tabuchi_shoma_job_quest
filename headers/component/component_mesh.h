#pragma once
#include"i_component.h"

struct ComponentMesh :public IComponent
{
    //メッシュ自体を持つのではなく、
    // メッシュが保存されている配列のどこにあるかを示す
    int mesh_id;
};
