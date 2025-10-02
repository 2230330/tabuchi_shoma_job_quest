#pragma once
#include<memory>
#include"../gltf_model.h"
#include"i_component.h"

struct ComponentGltf :public IComponent
{
    std::shared_ptr<GltfModel>model;
    size_t animation_index = 0;
    float animation_time = 0.0f;
};
