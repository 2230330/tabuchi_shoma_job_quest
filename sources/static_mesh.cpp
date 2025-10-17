#include"../headers/static_mesh.h"

#include<fstream>
#include<filesystem>

#include"../headers/misc.h"
#include"../headers/shader.h"
#include"../headers/resource_manager.h"

/*
* obj形式ファイルの読み込みと生成
* device : 使用するリソース
* obj_filename : 読み込むobj形式ファイル
* uv_flag : オブジェクトに描画するテクスチャファイルが裏返っているかどうかの確認
* count : インスタンス化しているため、何個呼び出すのか
*/
StaticMesh::StaticMesh(ID3D11Device* device, const wchar_t* obj_filename, bool uv_flag, UINT count)
{
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    uint32_t current_index{ 0 };

    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<DirectX::XMFLOAT3> normals;
    std::vector<DirectX::XMFLOAT2> texcoords;
    std::vector<std::wstring>  mtl_filenames;

    std::wifstream fin{};
    wchar_t command[256];
    //オブジェクトファイルの読み込み
    {
        fin.open(obj_filename);
        _ASSERT_EXPR(fin, L"OBJ file not found");

        while (fin)
        {
            fin >> command;
            if (0 == wcscmp(command, L"v"))
            {
                float x, y, z;
                fin >> x >> y >> z;
                positions.push_back({ x,y,z });
                fin.ignore(1024, L'\n');
            }
            else if (0 == wcscmp(command, L"vn"))
            {
                float i, j, k;
                fin >> i >> j >> k;
                normals.push_back({ i,j,k });
                fin.ignore(1024, L'\n');
            }
            else if (0 == wcscmp(command, L"vt"))
            {
                float u, v;
                fin >> u >> v;
                if (!uv_flag)texcoords.push_back({ u,v });
                else texcoords.push_back({ u,1.0f-v });
                fin.ignore(1024, L'\n');
            }
            else if (0 == wcscmp(command, L"f"))
            {
                for (size_t i = 0; i < 3; i++)
                {
                    Vertex vertex;
                    size_t v, vt, vn;

                    fin >> v;
                    vertex.position = positions.at(v - 1);
                    if (L'/' == fin.peek())
                    {
                        fin.ignore(1);
                        if (L'/' != fin.peek())
                        {
                            fin >> vt;
                            vertex.texcoord = texcoords.at(vt - 1);
                        }
                        if (L'/' == fin.peek())
                        {
                            fin.ignore(1);
                            fin >> vn;
                            vertex.normal = normals.at(vn - 1);
                        }
                    }
                    vertices.emplace_back(vertex);
                    indices.emplace_back(current_index++);
                }
                fin.ignore(1024, L'\n');
            }
            else if (0 == wcscmp(command, L"mtllib"))
            {
                wchar_t mtllib[256];
                fin >> mtllib;
                mtl_filenames.emplace_back(mtllib);
            }
            else if (0 == wcscmp(command, L"usemtl"))
            {
                wchar_t usemtl[MAX_PATH]{ 0 };
                fin >> usemtl;
                subsets_.push_back({ usemtl,static_cast<uint32_t>(indices.size()),0 });
            }
            else
            {
                fin.ignore(1024, L'\n');
            }
        }
        fin.close();

        std::vector<Subset>::reverse_iterator iterator = subsets_.rbegin();
        iterator->index_count = static_cast<uint32_t>(indices.size()) - iterator->index_start;
        auto size = subsets_.rend();
        for (iterator = subsets_.rbegin() + 1; iterator != size; ++iterator)
        {
            iterator->index_count = (iterator - 1)->index_start - iterator->index_start;
        }
    }
    //テクスチャファイルの読み込み
    {
        std::filesystem::path mtl_filename(obj_filename);
        mtl_filename.replace_filename(std::filesystem::path(mtl_filenames[0]).filename());

        fin.open(mtl_filename);
        _ASSERT_EXPR(fin, L"MTL file not found.");
        
        while (fin)
        {
            fin >> command;
            if (0 == wcscmp(command, L"map_Kd"))
            {
                fin.ignore();
                wchar_t map_Kd[256];
                fin >> map_Kd;

                std::filesystem::path path(obj_filename);
                path.replace_filename(std::filesystem::path(map_Kd).filename());
                materials_.rbegin()->texture_filenames[0] = path;
                fin.ignore(1024, L'\n');
            }
            else if (0 == wcscmp(command, L"map_bump") || 0 == wcscmp(command, L"bump"))
            {
                fin.ignore();
                wchar_t map_bump[256];
                fin >> map_bump;
                std::filesystem::path path(obj_filename);
                path.replace_filename(std::filesystem::path(map_bump).filename());
                materials_.rbegin()->texture_filenames[1] = path;
                fin.ignore(1024, L'\n');
            }
            else if (0 == wcscmp(command, L"newmtl"))
            {
                fin.ignore();
                wchar_t newmtl[256];
                Material material;

                fin >> newmtl;
                material.name = newmtl;
                materials_.push_back(material);
            }
            else if (0 == wcscmp(command, L"Ka"))
            {
                float r, g, b;
                fin >> r >> g >> b;
                materials_.rbegin()->Ka = { r,g,b,1 };
                fin.ignore(1024, L'\n');
            }
            else if (0 == wcscmp(command, L"Kd"))
            {
                float r, g, b;
                fin >> r >> g >> b;
                materials_.rbegin()->Kd = { r,g,b,1 };
                fin.ignore(1024, L'\n');
            }
            else if (0 == wcscmp(command, L"Ks"))
            {
                float r, g, b;
                fin >> r >> g >> b;
                materials_.rbegin()->Ks = { r,g,b,1 };
                fin.ignore(1024, L'\n');
            }
            else
            {
                fin.ignore(1024, L'\n');
            }
        }
        fin.close();
            
        {
            //MTLファイルがない時に背のファイルを作る
            if (materials_.size() == 0)
            {
                Subset* subset = subsets_.data();
                int size = static_cast<int>(subsets_.size());
                for (int i = 0; i < size; i++)
                {
                    materials_.emplace_back(subset[i].usemtl);
                }
            }

        }
        

        //シェーダーリソースビュー生成
        {
            D3D11_TEXTURE2D_DESC texture2d_desc{};
            int size = static_cast<int>(materials_.size());
            Material* material = materials_.data();
            for (int i = 0; i < size; ++i)
            {
                if (material[i].texture_filenames[0].size() > 0)
                {
                    material[i].shader_resource_views[0]=ResourceManager::Instance().
                        LoadTextureFromFile(device, material[i].texture_filenames[0].c_str(),
                        &texture2d_desc);
                }
                else
                {
                    material[i].shader_resource_views[0]= ResourceManager::Instance().
                        MakeDummyTexture(device, 0xFFFFFFFF, 16);
                }
                if (material[i].texture_filenames[1].size() > 0)
                {
                    material[i].shader_resource_views[1]=ResourceManager::Instance().
                        LoadTextureFromFile(device, material[i].texture_filenames[1].c_str(),
                        &texture2d_desc);
                }
                else
                {
                    material[i].shader_resource_views[1]= ResourceManager::Instance().
                        MakeDummyTexture(device, 0xFFFF7F7F, 16);
                }
            }
        }
    }
    //シェーダー、バッファの設定
    {
        for (UINT i =0; i < count; i++)
        {
            InstanceData instance;
            DirectX::XMMATRIX S = DirectX::XMMatrixScaling(1.0f,1.0f,1.0f);
            DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(0, 0, 0);
            DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(0,0,0);
            DirectX::XMStoreFloat4x4(&instance.instance_world, S* R* T);
            instance.instance_color = { 1,1,1,1 };

            instances_.emplace_back(instance);
        }

        this->indices_count_ = static_cast<UINT>(indices.size());
        CreateComBuffers(
            device,
            vertices.data(),
            static_cast<UINT>(vertices.size()),
            indices.data(), 
            this->indices_count_
        );

        D3D11_INPUT_ELEMENT_DESC input_element_desc[] =
        {
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
             0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
            //インスタンスデータ
            {"INSTANCE_WORLD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,0,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",3,DXGI_FORMAT_R32G32B32A32_FLOAT,1,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
        };
        shader_from_cso::CreateVsFromCso(device, "./resources/shader/static_mesh_vs.cso", vertex_shader_.GetAddressOf(),
            input_layout_.GetAddressOf(), input_element_desc, ARRAYSIZE(input_element_desc));
        shader_from_cso::CreatePsFromCso(device, "./resources/shader/static_mesh_ps.cso", pixel_shader_.GetAddressOf());    
    }
}

/*
@ 描画関数
@ immediate_context : 描画するリソース
@ color : 描画の色(そのうち他関数を作成する予定)
*/
void StaticMesh::Render(
    ID3D11DeviceContext* immediate_context,
    const DirectX::XMFLOAT4X4&world,
    const DirectX::XMFLOAT4& color,
    const UINT& instance_number
    )
{
    int instance_size = static_cast<int>(instances_.size());
    InstanceData* instance = instances_.data();
    if (instance_number)
    {
        instance[instance_number].instance_world = world;
    }
    //マップで更新
    UpdateInstanceBuffer(immediate_context, instances_);

    ID3D11Buffer* buffers[]{ vertex_buffer_.Get(),instance_buffer_.Get() };
    UINT strides[]{ sizeof(Vertex),sizeof(InstanceData) };
    UINT offsets[]{ 0,0 };
    {
        immediate_context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
        immediate_context->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
        immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        immediate_context->IASetInputLayout(input_layout_.Get());

        immediate_context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
        immediate_context->PSSetShader(pixel_shader_.Get(), nullptr, 0);
    }

    //サブセット単位で描画
    {
        int material_size = static_cast<int>(materials_.size());
        Material* material = materials_.data();
        int subset_size = static_cast<int>(subsets_.size());
        Subset* subset = subsets_.data();

        for (int j = 0; j < material_size; j++)
        {
            immediate_context->PSSetShaderResources(0, 1, material[j].shader_resource_views[0].GetAddressOf());
            immediate_context->PSSetShaderResources(1, 1, material[j].shader_resource_views[1].GetAddressOf());

            for (int i = 0; i < instance_size; i++)
            {
                DirectX::XMStoreFloat4(&instance[i].instance_color,
                    DirectX::XMVectorMultiply(
                        DirectX::XMLoadFloat4(&color)
                        , DirectX::XMLoadFloat4(&material[j].Kd)
                    )
                );
            }


            for (int sub = 0; sub < subset_size; sub++)
            {
                if (material[j].name == subset[sub].usemtl)
                {
                    immediate_context->DrawIndexedInstanced(
                        subset[sub].index_count,
                        instance_size,
                        subset[sub].index_start,
                        0,
                        0);
                }
            }
        }
    }
}


void __cdecl StaticMesh::CreateComBuffers(
    ID3D11Device* device,
    const Vertex* vertices,
    const UINT vertices_count,
    const uint32_t* indices,
    const UINT indices_count
)
{
    HRESULT hr{ S_OK };
    D3D11_BUFFER_DESC buffer_desc{};

    D3D11_SUBRESOURCE_DATA vertex_data{};
    buffer_desc.ByteWidth = (sizeof(Vertex) * static_cast<UINT>(vertices_count));
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;
    vertex_data.pSysMem = vertices;
    vertex_data.SysMemPitch = 0;
    vertex_data.SysMemSlicePitch = 0;
    hr = device->CreateBuffer(&buffer_desc, &vertex_data, vertex_buffer_.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    D3D11_SUBRESOURCE_DATA index_data{};
    buffer_desc.ByteWidth = (sizeof(uint32_t) * static_cast<UINT>(indices_count));
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    index_data.pSysMem = indices;
    index_data.SysMemPitch = 0;
    index_data.SysMemSlicePitch = 0;
    hr = device->CreateBuffer(&buffer_desc, &index_data, index_buffer_.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    //インスタンスバッファの設定
    buffer_desc.ByteWidth=(sizeof(InstanceData) * static_cast<UINT>(2048));
    buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = sizeof(InstanceData);
    hr = device->CreateBuffer(&buffer_desc, nullptr, instance_buffer_.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

/*
@Mapでインスタンスの情報を書き換える関数
@immeidate_context : 使うリソース
@instance : 更新したいインスタンス配列
*/
void StaticMesh::UpdateInstanceBuffer(
    ID3D11DeviceContext* immediate_context,
    const std::vector<InstanceData>& instance)
{
    D3D11_MAPPED_SUBRESOURCE mapped_subresource;
    HRESULT hr{ S_OK };
    hr = immediate_context->Map(instance_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    memcpy(mapped_subresource.pData, instance.data(), instance.size()*sizeof(InstanceData));
    immediate_context->Unmap(instance_buffer_.Get(), 0);
}



