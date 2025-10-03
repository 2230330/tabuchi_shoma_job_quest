#pragma once
#include<stdint.h>
#include"../archetype/archetype.h"

//Entityがどのアーキタイプに属しているか、チャンク内で何番目かを記録する構造体
struct EntityRecord
{
    Archetype* archetype;
    uint32_t index_in_chunk;
};