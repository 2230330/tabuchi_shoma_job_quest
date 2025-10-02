#include"../headers/geometric_primitive.h"


#include"../headers/misc.h"
#include"../headers/shader.h"

void GeometricPrimitive::Render(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4& color)
{
    //描画する頂点数やインスタンスを設定
    //インスタンスデータの更新
    int size = static_cast<int>(instances_.size());
    InstanceData* ins = instances_.data();
    for (int i = 0; i < size; i++)
    {
        ins->color = color;
    }

    UpdateInstances(immediate_context, instances_);

    ID3D11Buffer* buffers[]{ vertex_buffer_.Get(),instance_buffer_.Get() };
    UINT strides[] = { sizeof(Vertex),sizeof(InstanceData) };
    UINT offsets[] = { 0,0 };
    {
        //immediate_context->IASetVertexBuffers(0, 1, &buffers[0], &strides[0], &offsets[0]);
        //immediate_context->IASetVertexBuffers(1, 1, &buffers[1], &strides[1], &offsets[1]);
        immediate_context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
    }
    immediate_context->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    immediate_context->IASetInputLayout(input_layout_.Get());

    immediate_context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
    immediate_context->PSSetShader(pixel_shader_.Get(), nullptr, 0);

    immediate_context->DrawIndexedInstanced(indices_count_, static_cast<UINT>(instances_.size()), 0, 0, 0);
}

void GeometricPrimitive::CreateGeometricPrimitive(DirectX::XMFLOAT3 scale, DirectX::XMFLOAT3 rotation, DirectX::XMFLOAT3 position, DirectX::XMFLOAT4 color)
{
    DirectX::XMMATRIX S = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
    DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);
    DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);

    InstanceData new_instance;

    XMStoreFloat4x4(&new_instance.world, S * R * T);

    new_instance.color = color;

    instances_.emplace_back(new_instance);

}

void __cdecl GeometricPrimitive::CreateComBuffers(
    ID3D11Device* device,
    const Vertex* vertices,
    const UINT vertices_count,
    const uint32_t* indices,
    const UINT indices_count
)
{
    HRESULT hr{ S_OK };

    D3D11_BUFFER_DESC         buffer_desc{};

    //頂点バッファの設定
    D3D11_SUBRESOURCE_DATA    vertex_data{};
    buffer_desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices_count);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;
    vertex_data.pSysMem = vertices;
    vertex_data.SysMemPitch = 0;
    vertex_data.SysMemSlicePitch = 0;
    hr = device->CreateBuffer(&buffer_desc, &vertex_data, vertex_buffer_.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
  
    //インデックスバッファの設定
    D3D11_SUBRESOURCE_DATA    index_data{};
    buffer_desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * indices_count);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;
    index_data.pSysMem = indices;
    index_data.SysMemPitch = 0;
    index_data.SysMemSlicePitch = 0;
    hr = device->CreateBuffer(&buffer_desc, &index_data, index_buffer_.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

//内部でインスタンス配列のサイズを取っているからインスタンスの設定が終わった後に入れる事
void GeometricPrimitive::CreateInstanceBuffer(ID3D11Device* device)
{
    //インスタンス用バッファ
    HRESULT hr{ S_OK };
    
    D3D11_BUFFER_DESC instance_desc{};
    instance_desc.ByteWidth = 
        (sizeof(InstanceData) *static_cast<int>(2048));  //インスタンス配列のサイズを入れているので、配列の設定の後に入れてください。
    instance_desc.Usage = D3D11_USAGE_DYNAMIC;
    instance_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    instance_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    instance_desc.MiscFlags = 0;
    instance_desc.StructureByteStride = sizeof(InstanceData);
    hr = device->CreateBuffer(&instance_desc, nullptr, instance_buffer_.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

void GeometricPrimitive::UpdateInstances(ID3D11DeviceContext* immediate_context, const std::vector<InstanceData>& instances)
{
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    HRESULT hr{ S_OK };
    hr = immediate_context->Map(instance_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    memcpy(mapped_resource.pData, instances.data(), instances.size() * sizeof(InstanceData));
    immediate_context->Unmap(instance_buffer_.Get(),0);
}

GeometricCube::GeometricCube(ID3D11Device* device) :GeometricPrimitive(device)
{
    const  Vertex vertices[]=
    {
        {{-0.5f,0.5f,-0.5f},{0,0,-1}},
        {{0.5f,0.5f,-0.5f},{0,0,-1}},
        {{-0.5f,-0.5f,-0.5f},{0,0,-1}},
        {{0.5f,-0.5f,-0.5f},{0,0,-1}},

        {{-0.5f,0.5f,0.5f},{-1,0,0}},
        {{-0.5f,0.5f,-0.5f},{-1,0,0}},
        {{-0.5f,-0.5f,0.5f},{-1,0,0}},
        {{-0.5f,-0.5f,-0.5f},{-1,0,0}},

        {{-0.5f,0.5f,0.5f},{0,1,0}},
        {{0.5f,0.5f,0.5f},{0,1,0}},
        {{-0.5f,0.5f,-0.5f},{0,1,0}},
        {{0.5f,0.5f,-0.5f},{0,1,0}},

        {{0.5f,0.5f,-0.5f},{1,0,0}},
        {{0.5f,0.5f,0.5f},{1,0,0}},
        {{0.5f,-0.5f,-0.5f},{1,0,0}},
        {{0.5f,-0.5f,0.5f},{1,0,0}},

        {{0.5f,-0.5f,0.5f},{0,-1,0}},
        {{-0.5f,-0.5f,0.5f},{0,-1,0}},
        {{0.5f,-0.5f,-0.5f},{0,-1,0}},
        {{-0.5f,-0.5f,-0.5f},{0,-1,0}},

        {{0.5f,0.5f,0.5f},{0,0,1}},
        {{-0.5f,0.5f,0.5f},{0,0,1}},
        {{0.5f,-0.5f,0.5f},{0,0,1}},
        {{-0.5f,-0.5f,0.5f},{0,0,1}}

    };
    const uint32_t indices[]
    {
        0,1,2,    1,3,2,
        4,5,6,    5,7,6,
        8,9,10,   9,11,10,
        12,13,14, 13,15,14,
        16,17,18, 17,19,18,
        20,21,22, 21,23,22
    };

 

    HRESULT hr{ S_OK };

    D3D11_INPUT_ELEMENT_DESC input_element_desc[]
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
        //インスタンス用
        {"INSTANCE_WORLD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
        0,D3D11_INPUT_PER_INSTANCE_DATA,1},
        {"INSTANCE_WORLD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
        {"INSTANCE_WORLD",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
        {"INSTANCE_WORLD",3,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
        //インスタンスごとの色
        {"INSTANCE_COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
    };
    shader_from_cso::CreateVsFromCso(device, ".\\resources\\shader\\geometric_primitive_vs.cso", vertex_shader_.GetAddressOf(),
        input_layout_.GetAddressOf(), input_element_desc, ARRAYSIZE(input_element_desc));
    shader_from_cso::CreatePsFromCso(device, ".\\resources\\shader\\geometric_primitive_ps.cso", pixel_shader_.GetAddressOf());

    //バッファの生成
    {
        int vertices_count = sizeof(vertices) / sizeof(Vertex);
        int indices_count = sizeof(indices) / sizeof(uint32_t);
        this->indices_count_ = indices_count;
        //頂点バッファとインデックスバッファの設定
        CreateComBuffers(device, vertices,vertices_count, indices,indices_count);
        //インスタンス用バッファの設定
        CreateInstanceBuffer(device);

    }

}

GeometricSphere::GeometricSphere(
    ID3D11Device* device,
    float radius,
    unsigned int longitudeSegments,
    unsigned int latitudeSegments
):GeometricPrimitive(device)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (unsigned int y = 0; y <= latitudeSegments; ++y)
    {
        for (unsigned int x = 0; x <= longitudeSegments; ++x)
        {
            float xSegment = (float)x / (float)longitudeSegments;
            float ySegment = (float)y / (float)latitudeSegments;
            float xPos = radius * cosf(xSegment * DirectX::XM_2PI) * sinf(ySegment * DirectX::XM_PI);
            float yPos = radius * cosf(ySegment * DirectX::XM_PI);
            float zPos = radius * sinf(xSegment * DirectX::XM_2PI) * sinf(ySegment * DirectX::XM_PI);

            vertices.push_back({
                { xPos, yPos, zPos }, // position
                { xPos / radius, yPos / radius, zPos / radius }, // normal
                //{ xSegment, ySegment } // texCoord
                });
        }
    }

    for (unsigned int i = 0; i < latitudeSegments; ++i)
    {
        for (unsigned int j = 0; j < longitudeSegments; ++j)
        {
            unsigned int first = (i * (longitudeSegments + 1)) + j;
            unsigned int second = first + longitudeSegments + 1;

            indices.push_back(first);
            indices.push_back(first + 1);
            indices.push_back(second);

            indices.push_back(second);
            indices.push_back(first + 1);
            indices.push_back(second + 1);
        }
    }

    {
        HRESULT hr{ S_OK };
        D3D11_INPUT_ELEMENT_DESC input_element_desc[]
        {
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
            //インスタンス用
            {"INSTANCE_WORLD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            0,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",3,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            //インスタンスごとの色
            {"INSTANCE_COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
        };
        shader_from_cso::CreateVsFromCso(device, ".\\resources\\shader\\geometric_primitive_vs.cso", vertex_shader_.GetAddressOf(),
            input_layout_.GetAddressOf(), input_element_desc, ARRAYSIZE(input_element_desc));
        shader_from_cso::CreatePsFromCso(device, ".\\resources\\shader\\geometric_primitive_ps.cso", pixel_shader_.GetAddressOf());
        //バッファの生成
        {
            UINT vertices_count = static_cast<UINT>(vertices.size());
            UINT indices_count = static_cast<UINT>(indices.size());
            this->indices_count_ = indices_count;
            //頂点バッファとインデックスバッファの設定
            CreateComBuffers(device, vertices.data(), vertices_count, indices.data(), indices_count);
            //インスタンス用バッファの設定
            CreateInstanceBuffer(device);
        }
    }

}

GeometricCylinder::GeometricCylinder(
    ID3D11Device* device,
    float radius,
    float height,
    unsigned int sliceCount,
    unsigned int stackCount
):GeometricPrimitive(device)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.clear();
    indices.clear();

    float stackHeight = height / stackCount;
    float thetaStep = DirectX::XM_2PI / sliceCount;

    // 円柱の側面
    for (UINT i = 0; i <= stackCount; ++i)
    {
        float y = -0.5f * height + i * stackHeight;

        for (UINT j = 0; j <= sliceCount; ++j)
        {
            float theta = j * thetaStep;
            Vertex vertex;

            vertex.position = DirectX::XMFLOAT3(
                radius * cosf(theta),
                y,
                radius * sinf(theta)
            );

            vertex.normal = DirectX::XMFLOAT3(
                cosf(theta),
                0.0f,
                sinf(theta)
            );

            vertices.push_back(vertex);
        }
    }

    UINT ringVertexCount = sliceCount + 1;
    for (UINT i = 0; i < stackCount; ++i)
    {
        for (UINT j = 0; j < sliceCount; ++j)
        {
            indices.push_back(i * ringVertexCount + j);
            indices.push_back((i + 1) * ringVertexCount + j);
            indices.push_back((i + 1) * ringVertexCount + j + 1);

            indices.push_back(i * ringVertexCount + j);
            indices.push_back((i + 1) * ringVertexCount + j + 1);
            indices.push_back(i * ringVertexCount + j + 1);
        }
    }

    // 円柱の底面
    UINT baseIndex = static_cast<int>(vertices.size());
    vertices.push_back(Vertex{ DirectX::XMFLOAT3(0.0f, -0.5f * height, 0.0f), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f) });

    for (UINT i = 0; i <= sliceCount; ++i)
    {
        float theta = i * thetaStep;
        vertices.push_back(Vertex{ DirectX::XMFLOAT3(radius * cosf(theta), -0.5f * height, radius * sinf(theta)), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f) });
    }

    for (UINT i = 1; i <= sliceCount; ++i)
    {
        indices.push_back(baseIndex);
        indices.push_back(baseIndex + i);
        indices.push_back(baseIndex + i + 1);
    }

    // 円柱の上面
    baseIndex = vertices.size();
    vertices.push_back(Vertex{ DirectX::XMFLOAT3(0.0f, 0.5f * height, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) });

    for (UINT i = 0; i <= sliceCount; ++i)
    {
        float theta = i * thetaStep;
        vertices.push_back(Vertex{ DirectX::XMFLOAT3(radius * cosf(theta), 0.5f * height, radius * sinf(theta)), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) });
    }

    for (UINT i = 1; i <= sliceCount; ++i)
    {
        indices.push_back(baseIndex);
        indices.push_back(baseIndex + i + 1);
        indices.push_back(baseIndex + i);
    }

    {
        HRESULT hr{ S_OK };
        D3D11_INPUT_ELEMENT_DESC input_element_desc[]
        {
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
            //インスタンス用
            {"INSTANCE_WORLD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            0,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",3,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            //インスタンスごとの色
            {"INSTANCE_COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
        };
        shader_from_cso::CreateVsFromCso(device, ".\\resources\\shader\\geometric_primitive_vs.cso", vertex_shader_.GetAddressOf(),
            input_layout_.GetAddressOf(), input_element_desc, ARRAYSIZE(input_element_desc));
        shader_from_cso::CreatePsFromCso(device, ".\\resources\\shader\\geometric_primitive_ps.cso", pixel_shader_.GetAddressOf());
        //バッファの生成
        {
            UINT vertices_count = static_cast<UINT>(vertices.size());
            UINT indices_count = static_cast<UINT>(indices.size());
            this->indices_count_ = indices_count;
            //頂点バッファとインデックスバッファの設定
            CreateComBuffers(device, vertices.data(), vertices_count, indices.data(), indices_count);
            //インスタンス用バッファの設定
            CreateInstanceBuffer(device);
        }
    }
}

GeometricTorus::GeometricTorus(
    ID3D11Device* device,
    float outerRadius,
    float innerRadius,
    UINT sliceCount,
    UINT stackCount
):GeometricPrimitive(device)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float sliceStep = DirectX::XM_2PI / sliceCount;
    float stackStep = DirectX::XM_2PI / stackCount;

    // 頂点の生成
    for (UINT i = 0; i <= stackCount; ++i)
    {
        float phi = i * stackStep;
        float cosPhi = cosf(phi);
        float sinPhi = sinf(phi);

        for (UINT j = 0; j <= sliceCount; ++j)
        {
            float theta = j * sliceStep;
            float cosTheta = cosf(theta);
            float sinTheta = sinf(theta);

            Vertex vertex;

            vertex.position = DirectX::XMFLOAT3(
                (outerRadius + innerRadius * cosTheta) * cosPhi,
                innerRadius * sinTheta,
                (outerRadius + innerRadius * cosTheta) * sinPhi
            );

            vertex.normal = DirectX::XMFLOAT3(
                cosTheta * cosPhi,
                sinTheta,
                cosTheta * sinPhi
            );

            vertices.push_back(vertex);
        }
    }

    // インデックスの生成
    UINT ringVertexCount = sliceCount + 1;
    for (UINT i = 0; i < stackCount; ++i)
    {
        for (UINT j = 0; j < sliceCount; ++j)
        {
            indices.push_back(i * ringVertexCount + j);
            indices.push_back((i + 1) * ringVertexCount + j + 1);
            indices.push_back((i + 1) * ringVertexCount + j);

            indices.push_back(i * ringVertexCount + j);
            indices.push_back(i * ringVertexCount + j + 1);
            indices.push_back((i + 1) * ringVertexCount + j + 1);
        }
    }

    {
        HRESULT hr{ S_OK };
        D3D11_INPUT_ELEMENT_DESC input_element_desc[]
        {
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
            //インスタンス用
            {"INSTANCE_WORLD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            0,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"INSTANCE_WORLD",3,DXGI_FORMAT_R32G32B32A32_FLOAT,1,
            D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
            //インスタンスごとの色
            {"INSTANCE_COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_INSTANCE_DATA,1},
        };
        shader_from_cso::CreateVsFromCso(device, ".\\resources\\shader\\geometric_primitive_vs.cso", vertex_shader_.GetAddressOf(),
            input_layout_.GetAddressOf(), input_element_desc, ARRAYSIZE(input_element_desc));
        shader_from_cso::CreatePsFromCso(device, ".\\resources\\shader\\geometric_primitive_ps.cso", pixel_shader_.GetAddressOf());
        //バッファの生成
        {
            UINT vertices_count = static_cast<UINT>(vertices.size());
            UINT indices_count = static_cast<UINT>(indices.size());
            this->indices_count_ = indices_count;
            //頂点バッファとインデックスバッファの設定
            CreateComBuffers(device, vertices.data(), vertices_count, indices.data(), indices_count);
            //インスタンス用バッファの設定
            CreateInstanceBuffer(device);
        }
    }
}
