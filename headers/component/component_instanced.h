#pragma once
#include"i_component.h"
#include<stdint.h>

//インスタンシング描画の際、描画対象を識別するためのもの
struct ComponentInstanced
{
    uint32_t entity_id;
};