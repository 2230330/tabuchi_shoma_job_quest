#pragma once

enum  ConstantBufferSlot :int {
    kPerObject = 0,
    kPerFrame,
    kPerMaterial,
    kForwardLight,
    kDeferredLight,
    kPostEffect,
    empty,//現在空席
    kCamera,
    kCascadeShadow,
    kSsr,

    kCustomStart = 10,
    kSkyAtmosphere = kCustomStart + 1,
    kCloudDome = kCustomStart + 2,
    kPbrAjdjastParamter = kCustomStart + 3,
    

    kCount
};