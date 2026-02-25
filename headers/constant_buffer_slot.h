#pragma once
#include<stdint.h>

enum  ConstantBufferSlot :uint8_t {
    kPerObject = 0,
    kPerFrame,
    kPerMaterial,
    kForwardLight,
    kDeferredLight,
    kPostEffect,
    kTessellation,

    kCustomStart = 10,
    kSkyAtmosphere = kCustomStart + 1,
    kCloudDome = kCustomStart + 2,
    kPbrAjdjastParamter = kCustomStart + 3,
    

    kCount
};