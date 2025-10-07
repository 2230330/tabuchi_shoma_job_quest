#pragma once
#include"i_importer.h"
#include"../gltf_model.h"
#include"../world/world.h"

//コンポーネント指向用のGLTFインポーター
class GltfImporter :public IImporter
{
public:
    GltfImporter(World& world);
    void ImportRenderEntities(const GltfModel& model);

private:
    World& world_;
};