#pragma once
#include<memory>
#include<vector>
#include"../gltf_model.h"
#include"i_component.h"

struct ComponentGltf :public IComponent
{
    std::shared_ptr<GltfModel>model;
    size_t animation_index = 0;
    float animation_time = 0.0f;
    float animation_speed = 1.0f;
    bool loop = true;

    std::vector<GltfModel::Node>animated_nodes;

};
