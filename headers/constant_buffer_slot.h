#pragma once
#include<stdint.h>

enum class ConstantBufferSlot :uint8_t {
    kPerObject = 0,
    kPerFrame,
    kPerMaterial,
    kForwardLight,
    kDeferredLight,
    kPostEffect,

    kCustomStart = 10,
    kSkyRayleigh = kCustomStart + 1,
    kCloudDome = kCustomStart + 2,

    kCount
};