#include"../headers/skinned_mesh.h"

#include<Windows.h>
#include<sstream>
#include<functional>
#include<filesystem>
#include<fstream>

#include"fbxsdk/core/fbxproperty.h"
#include"../headers/misc.h"
#include"../headers/shader.h"
#include"../headers/resource_manager.h"
#include"../headers/constant_buffer_slot.h"

inline std::string ConvertWstringToString(const std::wstring& wstr)
{
    if (wstr.empty())
        return std::string();

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}
//fbxbatrix_to_xmfloat4x4
inline DirectX::XMFLOAT4X4 to_xmfloat4x4(const FbxAMatrix& fbxmatrix)
{
    DirectX::XMFLOAT4X4 xmfloat4x4;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            xmfloat4x4.m[row][column] = static_cast<float>(fbxmatrix[row][column]);
        }
    }
    return xmfloat4x4;
}
//fbxdouble3_to_xmfloat3
inline DirectX::XMFLOAT3 to_xmfloat3(const FbxDouble3& fbxdouble3)
{
    DirectX::XMFLOAT3 xmfloat3;
    xmfloat3.x = static_cast<float>(fbxdouble3[0]);
    xmfloat3.y = static_cast<float>(fbxdouble3[1]);
    xmfloat3.z = static_cast<float>(fbxdouble3[2]);
    return xmfloat3;
}
//fbxdouble4_to_xmfloat4
inline DirectX::XMFLOAT4 to_xmfloat4(const FbxDouble4& fbxdouble4)
{
    DirectX::XMFLOAT4 xmfloat4;
    xmfloat4.x = static_cast<float>(fbxdouble4[0]);
    xmfloat4.y = static_cast<float>(fbxdouble4[1]);
    xmfloat4.z = static_cast<float>(fbxdouble4[2]);
    xmfloat4.w = static_cast<float>(fbxdouble4[3]);
    return xmfloat4;
}

struct BoneInfluence
{
    uint32_t bone_index;
    float bone_weight;
};
using bone_influences_per_control_point = std::vector<BoneInfluence>;

void FetchBoneInfluence(const FbxMesh* fbx_mesh,
    std::vector<bone_influences_per_control_point>& bone_influences)
{
    const int control_points_const{ fbx_mesh->GetControlPointsCount() };
    bone_influences.resize(control_points_const);

    const int skin_count{ fbx_mesh->GetDeformerCount(FbxDeformer::eSkin) };
    for (int skin_index = 0; skin_index < skin_count; ++skin_index)
    {
        const FbxSkin* fbx_skin
        { static_cast<FbxSkin*>(fbx_mesh->GetDeformer(skin_index,FbxDeformer::eSkin)) };

        const int cluster_count{ fbx_skin->GetClusterCount() };
        for (int cluster_index = 0; cluster_index < cluster_count; ++cluster_index)
        {
            const FbxCluster* fbx_cluster{ fbx_skin->GetCluster(cluster_index) };

            const int control_point_indices_count{ fbx_cluster->GetControlPointIndicesCount() };
            for (int control_point_indices_index = 0; control_point_indices_index < control_point_indices_count;
                ++control_point_indices_index)
            {
                int control_point_index{ fbx_cluster->GetControlPointIndices()[control_point_indices_index] };
                double control_point_weight
                { fbx_cluster->GetControlPointWeights()[control_point_indices_index] };
                BoneInfluence& bone_influence{ bone_influences.at(control_point_index).emplace_back() };
                bone_influence.bone_index = static_cast<uint32_t>(cluster_index);
                bone_influence.bone_weight = static_cast<float>(control_point_weight);
            }
        }
    }
}

SkinnedMesh::SkinnedMesh(ID3D11Device* device, const wchar_t* fbx_filename, bool trianglate, int sampling_rate)
{
    //_ASSERT_EXPR(fbx_filename, "FBX file not found");
        std::ifstream test(fbx_filename);
    _ASSERT_EXPR(test.is_open(), "fbx file  don't open !");
    _ASSERT_EXPR(test.peek() != std::ifstream::traits_type::eof(), "fbx file is null !");
    
    std::filesystem::path cereal_filename(ConvertWstringToString(fbx_filename));
    cereal_filename.replace_extension("cereal");
    if (std::filesystem::exists(cereal_filename.c_str()))
    {
        std::ifstream ifs(cereal_filename.c_str(), std::ios::binary);
        _ASSERT_EXPR(ifs, "FBX file don't open");
        if (ifs.peek() == EOF) { assert("error: file is empty"); }
        cereal::BinaryInputArchive deserialization(ifs);
        deserialization(scene_view_, meshes_, materials_, animation_clips_); 

    }
    else
    {
        FetchScene(fbx_filename, trianglate, sampling_rate);

        std::ofstream ofs(cereal_filename.c_str(), std::ios::binary);
        cereal::BinaryOutputArchive serialization(ofs);
        serialization(scene_view_, meshes_, materials_, animation_clips_);
    }
    CreateComObjects(device, fbx_filename);
}

void SkinnedMesh::Render(ID3D11DeviceContext* immediate_context,
    const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4& material_color,
    const Animation::KeyFrame* keyframe)
{
    size_t meshes_size{ meshes_.size() };
    Mesh* mesh = meshes_.data();

    for (int mesh_index = 0; mesh_index < meshes_size; ++mesh_index)
    {
        uint32_t stride{ sizeof(Vertex) };
        uint32_t offset{ 0 };
        immediate_context->IASetVertexBuffers(0, 1, mesh[mesh_index].vertex_buffer.GetAddressOf(), &stride, &offset);
        immediate_context->IASetIndexBuffer(mesh[mesh_index].index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        immediate_context->IASetInputLayout(input_layout_.Get());

        immediate_context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
        immediate_context->PSSetShader(pixel_shader_.Get(), nullptr, 0);

        Constants data;
        //DirectX::XMStoreFloat4x4(&data.world, DirectX::XMLoadFloat4x4(&mesh[mesh_index].default_global_transform) * DirectX::XMLoadFloat4x4(&world));
        if (keyframe && keyframe->nodes.size() > 0)
        {
            const Animation::KeyFrame::Node& mesh_node{ keyframe->nodes.at(mesh[mesh_index].node_index) };
            DirectX::XMStoreFloat4x4(&data.world, DirectX::XMLoadFloat4x4(&mesh_node.global_transform) * DirectX::XMLoadFloat4x4(&world));

            const size_t bone_count{ mesh[mesh_index].bind_pose.bones.size() };
            _ASSERT_EXPR(bone_count < MAX_BONES, L"The value of the 'bone_count' has exceeded MAX_BONES.");

            for (size_t bone_index = 0; bone_index < bone_count; ++bone_index)
            {
                const Skeleton::Bone& bone{ mesh[mesh_index].bind_pose.bones.at(bone_index) };
                const Animation::KeyFrame::Node& bone_node{ keyframe->nodes.at(bone.node_index) };
                DirectX::XMStoreFloat4x4(&data.bone_transforms[bone_index],
                    DirectX::XMLoadFloat4x4(&bone.offset_transform) *
                    DirectX::XMLoadFloat4x4(&bone_node.global_transform) *
                    DirectX::XMMatrixInverse(nullptr,
                        DirectX::XMLoadFloat4x4(&mesh[mesh_index].default_global_transform))
                );
            }
        }
        else
        {
            DirectX::XMStoreFloat4x4(&data.world,
                DirectX::XMLoadFloat4x4(&mesh[mesh_index].default_global_transform) * DirectX::XMLoadFloat4x4(&world));
            for (size_t bone_index = 0; bone_index < MAX_BONES; ++bone_index)
            {
                data.bone_transforms[bone_index] = { 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 };
            }
        }

        size_t subsets_size{ mesh[mesh_index].subsets.size() };
        const Mesh::Subset* subset = mesh[mesh_index].subsets.data();
        for (size_t subset_index = 0; subset_index < subsets_size; ++subset_index)
        {
            const Material& material{ materials_.at(subset[subset_index].material_unique_id) };

            DirectX::XMStoreFloat4(&data.material_color, DirectX::XMVectorMultiply(DirectX::XMLoadFloat4(&material_color) , DirectX::XMLoadFloat4(&material.Kd)));
            immediate_context->UpdateSubresource(constant_buffer_.Get(), 0, 0, &data, 0, 0);
            immediate_context->VSSetConstantBuffers(static_cast<int>(ConstantBufferSlot::kPerObject), 1, constant_buffer_.GetAddressOf());

            immediate_context->PSSetShaderResources(0, 1, material.shader_resource_view[0].GetAddressOf());
            immediate_context->PSSetShaderResources(1, 1, material.shader_resource_view[1].GetAddressOf());

            immediate_context->DrawIndexed(subset[subset_index].index_count, subset[subset_index].start_index_location, 0);
        }
    }
}

void SkinnedMesh::FetchScene(const wchar_t* fbx_filename, bool trianglate = false, int sampling_rate = 0)
{
    //FBXマネージャーとシーンの作成
    FbxManager* fbx_manager{ FbxManager::Create() };
    FbxScene* fbx_scene{ FbxScene::Create(fbx_manager,"") };

    //wstring_to_string
    std::string fbx_filename_str = ConvertWstringToString(fbx_filename);
    const char* fbx_filename_cstr = fbx_filename_str.c_str();

    //FBXインポーターの初期化とシーンへのインポート
    FbxImporter* fbx_importer{ FbxImporter::Create(fbx_manager,"") };
    bool         import_status{ false };
    import_status = fbx_importer->Initialize(fbx_filename_cstr);
    _ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());

    import_status = fbx_importer->Import(fbx_scene);
    _ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());

    //ジオメトリの変換
    FbxGeometryConverter fbx_converter(fbx_manager);
    if (trianglate)
    {
        //もし真なら、メッシュを三角形化し、不正なポリゴンを消す
        fbx_converter.Triangulate(fbx_scene, true/*replace*/, false/*legacy*/);
        fbx_converter.RemoveBadPolygonsFromMeshes(fbx_scene);
    }

    //シーンのノードを再帰的に走査
    std::function<void(FbxNode*)> traverse{ [&](FbxNode* fbx_node)
        {
        SceneGraph::Node& node{scene_view_.nodes.emplace_back()};
        node.attribute = fbx_node->GetNodeAttribute() ?
            fbx_node->GetNodeAttribute()->GetAttributeType() : FbxNodeAttribute::EType::eUnknown;
        node.name = fbx_node->GetName();
        node.unique_id = fbx_node->GetUniqueID();
        node.parent_index = scene_view_.Indexof(fbx_node->GetParent() ?
            fbx_node->GetParent()->GetUniqueID() : 0);
        for (int child_index = 0; child_index < fbx_node->GetChildCount(); ++child_index)
        {
            traverse(fbx_node->GetChild(child_index));
        }
    } };
    traverse(fbx_scene->GetRootNode());

    //デバッグ用のノード情報の表示
#if _DEBUG
    const size_t nodes_size{ scene_view_.nodes.size() };
    const SceneGraph::Node* node = scene_view_.nodes.data();

    for (size_t node_index = 0; node_index < nodes_size; ++node_index)
    {
        FbxNode* fbx_node{ fbx_scene->FindNodeByName(node[node_index].name.c_str()) };
        //DIsplay node data in the output windo as debug
        std::string node_name = fbx_node->GetName();
        uint64_t uid = fbx_node->GetUniqueID();
        uint64_t parent_uid = fbx_node->GetParent() ? fbx_node->GetParent()->GetUniqueID() : 0;
        int32_t  type = fbx_node->GetNodeAttribute() ? fbx_node->GetNodeAttribute()->GetAttributeType() : FbxNodeAttribute::EType::eUnknown;

        std::stringstream debug_string;
        debug_string << node_name << ":" << uid << ":" << parent_uid << ":" << type << "\n";
        OutputDebugStringA(debug_string.str().c_str());
    }
#endif
    FetchMeshes(fbx_scene, meshes_);
    FetchMaterials(fbx_scene, materials_);
    FetchAnimations(fbx_scene, animation_clips_, static_cast<float>(sampling_rate));

    fbx_manager->Destroy();
}

void SkinnedMesh::FetchMeshes(FbxScene* fbx_scene, std::vector<Mesh>& meshes)
{
    const size_t nodes_size{ scene_view_.nodes.size() };
    const SceneGraph::Node* node = scene_view_.nodes.data();

    for (size_t node_index = 0; node_index < nodes_size; ++node_index)
    {
        if (node[node_index].attribute != FbxNodeAttribute::EType::eMesh)
        {
            continue;
        }
        FbxNode* fbx_node{ fbx_scene->FindNodeByName(node[node_index].name.c_str()) };
        FbxMesh* fbx_mesh{ fbx_node->GetMesh() };

        Mesh& mesh{ meshes.emplace_back() };
        mesh.unique_id = fbx_node->GetUniqueID();
        mesh.name = fbx_node->GetName();
        mesh.node_index = scene_view_.Indexof(mesh.unique_id);
        //Unit21
        mesh.default_global_transform = to_xmfloat4x4(fbx_node->EvaluateGlobalTransform());

        //Unit22
        std::vector<bone_influences_per_control_point>bone_influences;
        FetchBoneInfluence(fbx_mesh, bone_influences);
        //Unit24
        FetchSkeleton(fbx_mesh, mesh.bind_pose);

        //Unit20
        std::vector<Mesh::Subset>& subsets{ mesh.subsets };
        const int material_count{ fbx_mesh->GetNode()->GetMaterialCount() };
        subsets.resize(material_count > 0 ? material_count : 1);
        for (int material_index = 0; material_index < material_count; material_index++)
        {
            const FbxSurfaceMaterial* fbx_material{ fbx_mesh->GetNode()->GetMaterial(material_index) };
            subsets.at(material_index).material_name = fbx_material->GetName();
            subsets.at(material_index).material_unique_id = fbx_material->GetUniqueID();
        }
        if (material_count > 0)
        {
            const int polygon_count{ fbx_mesh->GetPolygonCount() };
            for (int polygon_index = 0; polygon_index < polygon_count; ++polygon_index)
            {
                const int material_index
                { fbx_mesh->GetElementMaterial()->GetIndexArray().GetAt(polygon_index) };
                subsets.at(material_index).index_count += 3;
            }
            uint32_t offset{ 0 };

            const size_t subsets_size{ subsets.size() };
            Mesh::Subset* subset = subsets.data();
            for (size_t subset_index = 0; subset_index < subsets_size; ++subset_index)
            {
                subset[subset_index].start_index_location = offset;
                offset += subset[subset_index].index_count;
                //This will be used as counter in the following procedures,reset to zero
                subset[subset_index].index_count = 0;
            }
        }

        const int polygon_count{ fbx_mesh->GetPolygonCount() };
        mesh.vertices.resize(polygon_count * 3LL);
        mesh.indices.resize(polygon_count * 3LL);

        FbxStringList uv_names;
        fbx_mesh->GetUVSetNames(uv_names);
        const FbxVector4* control_points{ fbx_mesh->GetControlPoints() };
        for (int polygon_index = 0; polygon_index < polygon_count; ++polygon_index)
        {
            //unit20
            const int material_index{ material_count > 0 ?
                                fbx_mesh->GetElementMaterial()->GetIndexArray().GetAt(polygon_index) : 0 };
            Mesh::Subset& subset{ subsets.at(material_index) };
            const uint32_t offset{ subset.start_index_location + subset.index_count };

            for (int position_in_polygon = 0; position_in_polygon < 3; ++position_in_polygon)
            {
                const int vertex_index{ polygon_index * 3 + position_in_polygon };

                Vertex vertex;
                const int polygon_vertex{ fbx_mesh->GetPolygonVertex(polygon_index,position_in_polygon) };
                vertex.position.x = static_cast<float>(control_points[polygon_vertex][0]);
                vertex.position.y = static_cast<float>(control_points[polygon_vertex][1]);
                vertex.position.z = static_cast<float>(control_points[polygon_vertex][2]);

                const bone_influences_per_control_point& influences_per_control_point
                { bone_influences.at(polygon_vertex) };
                const int influence_count = static_cast<int>(influences_per_control_point.size());
                for (size_t influence_index = 0; influence_index < influence_count; ++influence_index)
                {
                    if (influence_index < MAX_BONE_INFLUENCES)
                    {
                        vertex.bone_weights[influence_index] =
                            influences_per_control_point.at(influence_index).bone_weight;
                        vertex.bone_indices[influence_index] =
                            influences_per_control_point.at(influence_index).bone_index;
                    }
#if 1               
                    else
                    {
                        size_t minimum_value_index = 0;
                        float minimum_value = FLT_MAX;
                        for (size_t i = 0; i < MAX_BONE_INFLUENCES; ++i)
                        {
                            if (minimum_value > vertex.bone_weights[i])
                            {
                                minimum_value = vertex.bone_weights[i];
                                minimum_value_index = i;
                            }
                        }
                        vertex.bone_weights[minimum_value_index] +=
                            influences_per_control_point.at(influence_index).bone_weight;
                        vertex.bone_indices[minimum_value_index] =
                            influences_per_control_point.at(influence_index).bone_index;
                    }
#endif
                }

                float total_weight = 0;
                for (size_t i = 0; i < MAX_BONE_INFLUENCES; ++i)
                {
                    total_weight += vertex.bone_weights[i];
                }

                for (size_t i = 0; i < MAX_BONE_INFLUENCES; ++i)
                {
                    vertex.bone_weights[i] /= total_weight;
                }
                if (fbx_mesh->GenerateNormals(false))
                {
                    FbxVector4 normal;
                    fbx_mesh->GetPolygonVertexNormal(polygon_index, position_in_polygon, normal);
                    vertex.normal.x = static_cast<float>(normal[0]);
                    vertex.normal.y = static_cast<float>(normal[1]);
                    vertex.normal.z = static_cast<float>(normal[2]);
                }
                if (fbx_mesh->GetElementUVCount() > 0)
                {
                    FbxVector2 uv;
                    bool unmapped_uv;
                    fbx_mesh->GetPolygonVertexUV(polygon_index, position_in_polygon,
                        uv_names[0], uv, unmapped_uv);
                    vertex.texcoord.x = static_cast<float>(uv[0]);
                    vertex.texcoord.y = 1.0f - static_cast<float>(uv[1]);
                }
                //Unit29
                if (fbx_mesh->GenerateTangentsData(0, false))
                {
                    const FbxGeometryElementTangent* tangent = fbx_mesh->GetElementTangent(0);
                    vertex.tangent.x = static_cast<float>(tangent->GetDirectArray().GetAt(vertex_index)[0]);
                    vertex.tangent.y = static_cast<float>(tangent->GetDirectArray().GetAt(vertex_index)[1]);
                    vertex.tangent.z = static_cast<float>(tangent->GetDirectArray().GetAt(vertex_index)[2]);
                    vertex.tangent.w = static_cast<float>(tangent->GetDirectArray().GetAt(vertex_index)[3]);
                }

                mesh.vertices.at(vertex_index) = std::move(vertex);
                mesh.indices.at(static_cast<size_t>(offset) + position_in_polygon) = vertex_index;
                subset.index_count++;
            }
        }

        const size_t vertices_size{ mesh.vertices.size() };
        Vertex* vertex = mesh.vertices.data();
        for (size_t vertex_index = 0; vertex_index < vertices_size; ++vertex_index)
        {
            mesh.bounding_box[0].x = std::min<float>(mesh.bounding_box[0].x, vertex[vertex_index].position.x);
            mesh.bounding_box[0].y = std::min<float>(mesh.bounding_box[0].y, vertex[vertex_index].position.y);
            mesh.bounding_box[0].z = std::min<float>(mesh.bounding_box[0].z, vertex[vertex_index].position.z);
            mesh.bounding_box[1].x = std::max<float>(mesh.bounding_box[1].x, vertex[vertex_index].position.x);
            mesh.bounding_box[1].y = std::max<float>(mesh.bounding_box[1].y, vertex[vertex_index].position.y);
            mesh.bounding_box[1].z = std::max<float>(mesh.bounding_box[1].z, vertex[vertex_index].position.z);
        }
    }
}

void SkinnedMesh::CreateComObjects(ID3D11Device* device, const wchar_t* fbx_filename)
{
    const size_t meshes_size{ meshes_.size() };
    Mesh* mesh = meshes_.data();

    for (size_t mesh_index = 0; mesh_index < meshes_size; ++mesh_index)
    {
        HRESULT hr{ S_OK };
        D3D11_BUFFER_DESC           buffer_desc{};
        D3D11_SUBRESOURCE_DATA      subresource_data{};
        buffer_desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * mesh[mesh_index].vertices.size());
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        buffer_desc.CPUAccessFlags = 0;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;
        subresource_data.pSysMem = mesh[mesh_index].vertices.data();
        subresource_data.SysMemPitch = 0;
        subresource_data.SysMemSlicePitch = 0;
        hr = device->CreateBuffer(&buffer_desc, &subresource_data,
            mesh[mesh_index].vertex_buffer.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        buffer_desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh[mesh_index].indices.size());
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        subresource_data.pSysMem = mesh[mesh_index].indices.data();
        hr = device->CreateBuffer(&buffer_desc, &subresource_data,
            mesh[mesh_index].index_buffer.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

#if 1
        mesh[mesh_index].vertices.clear();
        mesh[mesh_index].indices.clear();
#endif
    }

    std::unordered_map<uint64_t, Material>::iterator end = materials_.end();
    for (std::unordered_map<uint64_t, Material>::iterator iterator = materials_.begin(); iterator != end; ++iterator)
    {
        for (size_t texture_index = 0; texture_index < 2; ++texture_index)
        {
            if (iterator->second.texture_filenames[texture_index].size() > 0)
            {
                std::filesystem::path path(fbx_filename);
                path.replace_filename(iterator->second.texture_filenames[texture_index]);
                D3D11_TEXTURE2D_DESC texture2d_desc;
                iterator->second.shader_resource_view[texture_index]=ResourceManager::Instance().
                    LoadTextureFromFile(device, path.c_str());
                texture2d_desc = ResourceManager::Instance().
                    Texture2dDesc(iterator->second.shader_resource_view[texture_index].Get());
                
            }
            else
            {
                iterator->second.shader_resource_view[texture_index] = ResourceManager::Instance().MakeDummyTexture(device, texture_index == 1 ? 0xFFFF7F7F : 0xFFFFFFFF, 16);
            }
        }
    }
    //materialsに何も入ってないときダミーを突っ込んでみようと思います。
    if (materials_.size()<=0)
    {
        Material dummy_material;
        dummy_material.shader_resource_view[0]= ResourceManager::Instance().MakeDummyTexture(device, 0xFFFFFFFF, 16);
        dummy_material.shader_resource_view[1]= ResourceManager::Instance().MakeDummyTexture(device, 0xFFFFFFFF, 16);
        dummy_material.shader_resource_view[2]= ResourceManager::Instance().MakeDummyTexture(device, 0xFFFFFFFF, 16);
        materials_.insert({ 0,dummy_material });
    }


    HRESULT hr = S_OK;
    D3D11_INPUT_ELEMENT_DESC input_element_desc[]
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D11_APPEND_ALIGNED_ELEMENT},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D11_APPEND_ALIGNED_ELEMENT},
        {"TANGENT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,D3D11_APPEND_ALIGNED_ELEMENT},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D11_APPEND_ALIGNED_ELEMENT},
        {"WEIGHTS",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,D3D11_APPEND_ALIGNED_ELEMENT},
        {"BONES",0,DXGI_FORMAT_R32G32B32A32_UINT,0,D3D11_APPEND_ALIGNED_ELEMENT},
    };
    shader_from_cso::CreateVsFromCso(device, "./resources/shader/skinned_mesh_vs.cso", vertex_shader_.ReleaseAndGetAddressOf(),
        input_layout_.ReleaseAndGetAddressOf(), input_element_desc, ARRAYSIZE(input_element_desc));
    shader_from_cso::CreatePsFromCso(device, "./resources/shader/skinned_mesh_ps.cso", pixel_shader_.ReleaseAndGetAddressOf());

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(Constants);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer_.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

void SkinnedMesh::FetchMaterials(FbxScene* fbx_scene,
    std::unordered_map<uint64_t, Material>& materials)
{
    const size_t node_count{ scene_view_.nodes.size() };
    for (int node_index = 0; node_index < node_count; ++node_index)
    {
        const SceneGraph::Node& node{ scene_view_.nodes.at(node_index) };
        const FbxNode* fbx_node{ fbx_scene->FindNodeByName(node.name.c_str()) };

        const int material_count{ fbx_node->GetMaterialCount() };
        for (int material_index = 0; material_index < material_count; ++material_index)
        {
            const FbxSurfaceMaterial* fbx_material{ fbx_node->GetMaterial(material_index) };

            Material material;
            material.name = fbx_material->GetName();
            material.unique_id = fbx_material->GetUniqueID();
            FbxProperty fbx_property;
            //fbx_materialからDiffuseのプロパティを見つけてきている。
            fbx_property = fbx_material->FindProperty(FbxSurfaceMaterial::sDiffuse);
            if (fbx_property.IsValid())
            {
                const FbxDouble3 color{ fbx_property.Get<FbxDouble3>() };
                material.Kd.x = static_cast<float>(color[0]);
                material.Kd.y = static_cast<float>(color[1]);
                material.Kd.z = static_cast<float>(color[2]);
                material.Kd.w = 1.0f;

                const FbxFileTexture* fbx_texture{ fbx_property.GetSrcObject<FbxFileTexture>() };
                material.texture_filenames[0] =
                    fbx_texture ? fbx_texture->GetRelativeFileName() : "";
            }
            //fbx_propertyにAmbientを突っ込んでアンビエントを設定してみる
            fbx_property = fbx_material->FindProperty(FbxSurfaceMaterial::sAmbient);
            if (fbx_property.IsValid())
            {
                const FbxDouble3 color{ fbx_property.Get<FbxDouble3>() };
                material.Ka.x = static_cast<float>(color[0]);
                material.Ka.y = static_cast<float>(color[1]);
                material.Ka.z = static_cast<float>(color[2]);
                material.Ka.w = 1.0f;
            }
            //fbx_propertyにSpecularを突っ込めないかが策してみる
            fbx_property = fbx_material->FindProperty(FbxSurfaceMaterial::sSpecular);
            if (fbx_property.IsValid())
            {
                const FbxDouble3 color{ fbx_property.Get<FbxDouble3>() };
                material.Ks.x = static_cast<float>(color[0]);
                material.Ks.y = static_cast<float>(color[1]);
                material.Ks.z = static_cast<float>(color[2]);
                material.Ks.w = 1.0f;
            }
            fbx_property = fbx_material->FindProperty(FbxSurfaceMaterial::sNormalMap);
            if (fbx_property.IsValid())
            {
                const FbxFileTexture* fbx_texture{ fbx_property.GetSrcObject <FbxFileTexture>() };
                material.texture_filenames[1] =
                    fbx_texture ? fbx_texture->GetRelativeFileName() : "";
            }
            materials.emplace(material.unique_id, std::move(material));
        }
    }
#if 1
    // Append default(dummy) material
    materials.emplace();
#endif
}

void SkinnedMesh::FetchSkeleton(FbxMesh* fbx_mesh, Skeleton& bind_pose)
{
    const int deformer_count = fbx_mesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int deformer_index = 0; deformer_index < deformer_count; ++deformer_index)
    {
        FbxSkin* skin = static_cast<FbxSkin*>(fbx_mesh->GetDeformer(deformer_index, FbxDeformer::eSkin));
        const int cluster_count = skin->GetClusterCount();
        bind_pose.bones.resize(cluster_count);
        for (int cluster_index = 0; cluster_index < cluster_count; ++cluster_index)
        {
            FbxCluster* cluster = skin->GetCluster(cluster_index);

            Skeleton::Bone& bone{ bind_pose.bones.at(cluster_index) };
            FbxNode* cluster_link = cluster->GetLink();
            bone.name = cluster_link->GetName();
            bone.unique_id = cluster_link->GetUniqueID();
            bone.parent_index = bind_pose.Indexof(cluster_link->GetParent()->GetUniqueID());
            bone.node_index = scene_view_.Indexof(bone.unique_id);

            //'reference_global_init_position' is used to convert from local space of model(mesh) to
            //global space of scene.
            FbxAMatrix reference_global_init_position;
            cluster->GetTransformMatrix(reference_global_init_position);

            //'cluster_global_init_position' is used to convert from local space of bone to
            //global space of scene.
            FbxAMatrix cluster_global_init_position;
            cluster->GetTransformLinkMatrix(cluster_global_init_position);

            //Matrices are defined using the Column Major scheme. When a FbxAMatrix represents a transformation
            //(translation, rotation and scale), the last row of the matrix represents the translation part of
            //the transformation.
            //Compose 'bone.offset_transform' matrix that transforms position form mesh space to bone space.
            //This matrix is called the offset matrix.
            bone.offset_transform
                = to_xmfloat4x4(cluster_global_init_position.Inverse() * reference_global_init_position);
        }
    }
}

void SkinnedMesh::FetchAnimations(FbxScene* fbx_scene, std::vector<Animation>& animation_clips,
    float sampling_rate)
{
    FbxArray<FbxString*>animation_stack_names;
    fbx_scene->FillAnimStackNameArray(animation_stack_names);
    const int animation_stack_count{ animation_stack_names.GetCount() };
    for (int animation_stack_index = 0; animation_stack_index < animation_stack_count; ++animation_stack_index)
    {
        Animation& animation_clip{ animation_clips.emplace_back() };
        animation_clip.name = animation_stack_names[animation_stack_index]->Buffer();

        FbxAnimStack* animation_stack{ fbx_scene->FindMember<FbxAnimStack>(animation_clip.name.c_str()) };
        fbx_scene->SetCurrentAnimationStack(animation_stack);

        const FbxTime::EMode time_mode{ fbx_scene->GetGlobalSettings().GetTimeMode() };
        FbxTime one_second;
        one_second.SetTime(0, 0, 1, 0, 0, time_mode);
        animation_clip.sampling_rate = sampling_rate > 0 ?
            sampling_rate : static_cast<float>(one_second.GetFrameRate(time_mode));
        const FbxTime sampling_interval
        { static_cast<FbxLongLong>(one_second.Get() / animation_clip.sampling_rate) };

        const FbxTakeInfo* take_info{ fbx_scene->GetTakeInfo(animation_clip.name.c_str()) };
        const FbxTime start_time{ take_info->mLocalTimeSpan.GetStart() };
        const FbxTime stop_time{ take_info->mLocalTimeSpan.GetStop() };
        for (FbxTime time = start_time; time < stop_time; time += sampling_interval)
        {
            Animation::KeyFrame& keyframe{ animation_clip.sequence.emplace_back() };

            const size_t node_count{ scene_view_.nodes.size() };
            keyframe.nodes.resize(node_count);
            for (size_t node_index = 0; node_index < node_count; ++node_index)
            {
                FbxNode* fbx_node{ fbx_scene->FindNodeByName(scene_view_.nodes.at(node_index).name.c_str()) };
                if (fbx_node)
                {
                    Animation::KeyFrame::Node& node{ keyframe.nodes.at(node_index) };
                    //'global_transform' is a transformation matrix of a node with respect to
                    //the scene's global coordinate system.
                    node.global_transform = to_xmfloat4x4(fbx_node->EvaluateGlobalTransform(time));

                    //Unit27
                    //'local_transform' is a transformation matrix of a node with respect to
                    //its parent's local coordinate system.
                    const FbxAMatrix& local_transform{ fbx_node->EvaluateLocalTransform(time) };
                    node.scaling = to_xmfloat3(local_transform.GetS());
                    node.rotation = to_xmfloat4(local_transform.GetQ());
                    node.translation = to_xmfloat3(local_transform.GetT());
                }
            }
        }
    }
    for (int animation_stack_index = 0; animation_stack_index < animation_stack_count; ++animation_stack_index)
    {
        delete animation_stack_names[animation_stack_index];
    }
}

void SkinnedMesh::UpdateAnimation(Animation::KeyFrame& keyframe)
{
    size_t node_count{ keyframe.nodes.size() };
    for (size_t node_index = 0; node_index < node_count; ++node_index)
    {
        Animation::KeyFrame::Node& node{ keyframe.nodes.at(node_index) };
        DirectX::XMMATRIX S{ DirectX::XMMatrixScaling(node.scaling.x,node.scaling.y,node.scaling.z) };
        DirectX::XMMATRIX R{ DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation)) };
        DirectX::XMMATRIX T{ DirectX::XMMatrixTranslation(node.translation.x,node.translation.y,node.translation.z) };

        int64_t parent_index{ scene_view_.nodes.at(node_index).parent_index };
        DirectX::XMMATRIX P{ parent_index < 0 ? DirectX::XMMatrixIdentity() :
                                            DirectX::XMLoadFloat4x4(&keyframe.nodes.at(parent_index).global_transform) };

        DirectX::XMStoreFloat4x4(&node.global_transform, S * R * T * P);
    }
}

bool SkinnedMesh::UppendAnimations(const char* animation_filename, float sampling_rate)
{
    FbxManager* fbx_manager{ FbxManager::Create() };
    FbxScene* fbx_scene{ FbxScene::Create(fbx_manager,"") };

    FbxImporter* fbx_importer{ FbxImporter::Create(fbx_manager,"") };
    bool import_status{ false };
    import_status = fbx_importer->Initialize(animation_filename);
    _ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());
    import_status = fbx_importer->Import(fbx_scene);
    _ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());

    FetchAnimations(fbx_scene, animation_clips_, sampling_rate);

    fbx_manager->Destroy();

    return true;
}

void SkinnedMesh::BlendAnimations(const Animation::KeyFrame* keyframes[2], float factor,
    Animation::KeyFrame& keyframe)
{
    size_t node_count{ keyframes[0]->nodes.size() };
    keyframe.nodes.resize(node_count);
    for (size_t node_index = 0; node_index < node_count; ++node_index)
    {
        DirectX::XMVECTOR S[2]{
            DirectX::XMLoadFloat3(&keyframes[0]->nodes.at(node_index).scaling),
            DirectX::XMLoadFloat3(&keyframes[1]->nodes.at(node_index).scaling)
        };
        DirectX::XMStoreFloat3(&keyframe.nodes.at(node_index).scaling, DirectX::XMVectorLerp(S[0], S[1], factor));

        DirectX::XMVECTOR R[2]{
            DirectX::XMLoadFloat4(&keyframes[0]->nodes.at(node_index).rotation),
            DirectX::XMLoadFloat4(&keyframes[1]->nodes.at(node_index).rotation),
        };
        DirectX::XMStoreFloat4(&keyframe.nodes.at(node_index).rotation, DirectX::XMQuaternionSlerp(R[0], R[1], factor));

        DirectX::XMVECTOR T[2]{
            DirectX::XMLoadFloat3(&keyframes[0]->nodes.at(node_index).translation),
            DirectX::XMLoadFloat3(&keyframes[1]->nodes.at(node_index).translation),
        };
        DirectX::XMStoreFloat3(&keyframe.nodes.at(node_index).translation, DirectX::XMVectorLerp(T[0], T[1], factor));
    }
}
