#pragma once
#include"i_component.h"

//メッシュのプリミティブの為に作り出されたコンポーネント
//将来的に、タンジェントやスキンやジョイントが追加されたときにやりやすいかなと重い実装
struct ComponentPrimitive :public IComponent
{
    int primitive_index;
};