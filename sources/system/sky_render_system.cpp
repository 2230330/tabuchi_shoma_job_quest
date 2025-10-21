#include"../../headers/system/sky_render_system.h"
#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
#include"../../headers/render_state.h"
#include"../../headers/misc.h"

SkyRenderSystem::SkyRenderSystem(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();

    // 頂点生成
    for (unsigned int y = 0; y <= latitude_segments_; ++y) {
        for (unsigned int x = 0; x <= longitude_segments_; ++x) {
            float xSegment = static_cast<float>(x) / longitude_segments_;
            float ySegment = static_cast<float>(y) / latitude_segments_;

            float xPos = radius_ * cosf(xSegment * DirectX::XM_2PI) * sinf(ySegment * DirectX::XM_PI);
            float yPos = radius_ * cosf(ySegment * DirectX::XM_PI);
            float zPos = radius_ * sinf(xSegment * DirectX::XM_2PI) * sinf(ySegment * DirectX::XM_PI);

            vertices_.push_back({
                { xPos, yPos, zPos },
                { xSegment, ySegment }
                });
        }
    }

    // インデックス生成
    for (unsigned int i = 0; i < latitude_segments_; ++i) {
        for (unsigned int j = 0; j < longitude_segments_; ++j) {
            unsigned int first = (i * (longitude_segments_ + 1)) + j;
            unsigned int second = first + longitude_segments_ + 1;

            indices_.emplace_back(first);
            indices_.emplace_back(second);
            indices_.emplace_back(first + 1);

            indices_.emplace_back(first + 1);
            indices_.emplace_back(second);
            indices_.emplace_back(second + 1);
        }
    }

    // 頂点バッファ作成
    D3D11_BUFFER_DESC vb_desc{};
    vb_desc.Usage = D3D11_USAGE_DEFAULT;
    vb_desc.ByteWidth = static_cast<UINT>(sizeof(SkyVertex) * vertices_.size());
    vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vb_desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA vb_data{};
    vb_data.pSysMem = vertices_.data();

    HRESULT hr = device->CreateBuffer(&vb_desc, &vb_data, vertex_buffer_.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // インデックスバッファ作成
    D3D11_BUFFER_DESC ib_desc{};
    ib_desc.Usage = D3D11_USAGE_DEFAULT;
    ib_desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * indices_.size());
    ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ib_desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA ib_data{};
    ib_data.pSysMem = indices_.data();

    hr = device->CreateBuffer(&ib_desc, &ib_data, index_buffer_.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    index_count_ = static_cast<UINT>(indices_.size());

    // InputLayoutとシェーダーの読み込み（仮）
    D3D11_INPUT_ELEMENT_DESC input_element_desc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    sky_vs_ = ResourceManager::Instance().LoadVertexShader(device, L".\\resources\\shader\\sky_atmosphere_vs.cso",
        sky_input_.ReleaseAndGetAddressOf(), input_element_desc, _countof(input_element_desc));
    sky_ps_ = ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\sky_atmosphere_ps.cso");
}


void SkyRenderSystem::Render()
{
    comp_mng_.ForEach<ComponentSkyAtmosphere>([&](uint32_t entity_id, ComponentSkyAtmosphere&) {

        ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

        RenderState render_state(Graphics::Instance().GetDevice());
        // 深度・カリング設定（球の内側を描画）
        render_state.GetDepthStencilState(DepthState::no_test_no_write);
        render_state.GetRasterizerState(RasterizerState::solid_cull_none);

        // シェーダー設定
        context->VSSetShader(sky_vs_.Get(), nullptr, 0);
        context->PSSetShader(sky_ps_.Get(), nullptr, 0);
        context->IASetInputLayout(sky_input_.Get());

        // 頂点・インデックスバッファ設定
        UINT stride = sizeof(DirectX::XMFLOAT3) + sizeof(DirectX::XMFLOAT2); // SkyVertex構造に合わせる
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, vertex_buffer_.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // テクスチャ設定（必要なら）
        auto* texture = comp_mng_.TryGetByEntity<ComponentTexture>(entity_id);
        if (texture)
        {
         context->PSSetShaderResources(0, 1, texture->texture.GetAddressOf());
        }

        // 描画呼び出し
        context->DrawIndexed(index_count_, 0, 0);

        // シェーダー解除（任意）
        context->VSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(nullptr, nullptr, 0);
        ID3D11ShaderResourceView* null_shader[]{ nullptr };
        context->PSSetShaderResources(0, 1, null_shader);
        });
}
