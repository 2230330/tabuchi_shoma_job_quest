#pragma once
#include<stdint.h>

enum class ConstantBufferSlot :uint8_t {
    kPerObject = 0,
    kPerFrame,
    kPerMaterial,
    kLight,
    kPostEffect,

    kCustomStart = 10,
    kSkyRayleigh = kCustomStart + 0,

    kCount
};