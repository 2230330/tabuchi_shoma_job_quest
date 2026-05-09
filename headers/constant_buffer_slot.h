#pragma once

enum  ConstantBufferSlot :int {
    kPerObject = 0,
    kPerFrame,
    kPerMaterial,
    kForwardLight,
    kDeferredLight,
    kPostEffect,
    kHiz,
    kCamera,
    kCascadeShadow,
    kSsr,

    kCustomStart = 10,
    kSkyAtmosphere = kCustomStart + 1,
    kCloudDome = kCustomStart + 2,
    kPbrAjdjastParamter = kCustomStart + 3,
    

    kCount
};