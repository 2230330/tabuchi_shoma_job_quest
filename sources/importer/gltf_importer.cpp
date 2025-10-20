#include"../../headers/importer/gltf_importer.h"
#include"../../headers/component/component_mesh.h"
#include"../../headers/component/component_primitive.h"
#include"../../headers/component/component_material.h"
#include"../../headers/component/component_texture.h"

GltfImporter::GltfImporter(World& world)
    :world_(world)
{
}

//元々作っていたGLTFモデルを分解するような形でコンポーネント指向に充てる。
//これってインポーターって言っていいのかな？
void GltfImporter::ImportRenderEntities(const GltfModel& model) {
    //const auto& nodes = model.GetNodes();
    //const auto& meshes = model.GetMeshes();
    //const auto& materials = model.GetMaterials();

    //for (const auto& node : nodes) {
    //    if (node.mesh < 0) continue;

    //    const auto& mesh = meshes[node.mesh];
    //    for (size_t p = 0; p < mesh.primitives.size(); ++p) {
    //        const auto& primitive = mesh.primitives[p];
    //        int material_id = primitive.material;
    //        int texture_id = materials[material_id].data.pbr_metallic_roughness.basecolor_texture.index;

    //        std::set<std::type_index> types = {
    //            typeid(ComponentMesh),
    //            typeid(ComponentMaterial),
    //            typeid(ComponentTexture),
    //            typeid(ComponentPrimitive)
    //        };

    //        uint32_t entity_id = world_.GetEntityManager()->Add();
    //        Archetype* archetype = world_.GetArcheTypeManager()->GetOrCreateArchetype( types);
    //        uint32_t index = archetype->GetEntityList().size() - 1;

    //        ComponentMesh comp_mesh;
    //        comp_mesh.mesh_id = node.mesh;
    //        archetype->AddComponent<ComponentMesh>(index, comp_mesh);
    //        ComponentMaterial comp_material;
    //        comp_material.material_id = material_id;
    //        archetype->AddComponent<ComponentMaterial>(index, comp_material);
    //        ComponentTexture comp_texture;
    //        //comp_texture.texture_id = texture_id;
    //        archetype->AddComponent<ComponentTexture>(index, comp_texture);
    //        ComponentPrimitive comp_primitive;
    //        comp_primitive.primitive_index = static_cast<int>(p);
    //        archetype->AddComponent<ComponentPrimitive>(index, comp_primitive);
    //    }
    //}
}
