#include"../../headers/system/sky_render_system.h"

#include <DirectXTex.h>

#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
#include"../../headers/render_state.h"
#include"../../headers/misc.h"
#include"../../headers/constant_buffer_slot.h"

SkyRenderSystem::SkyRenderSystem(ComponentManager& comp_mng, RenderPass render_pass)
    :comp_mng_(comp_mng)
    , IRenderSystem(render_pass)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();

    //Texture2D/SRV
    {

    }

    //頂点情報とインデックス情報の作成
    {
        float phi_step{ DirectX::XM_PI / latitude_segments_ };
        float theta_step{ DirectX::XM_2PI / longitude_segments_ };
        // 頂点生成
        for (unsigned int lat = 0; lat <= latitude_segments_; ++lat) {
            float v = static_cast<float>(lat) / latitude_segments_;
            float phi = v * DirectX::XM_PI; // 緯度（0〜π）

            for (unsigned int lon = 0; lon <= longitude_segments_; ++lon) {
                float u = static_cast<float>(lon) / longitude_segments_;
                float theta = u * DirectX::XM_2PI; // 経度（0〜2π）

                float x = radius_ * sinf(phi) * cosf(theta);
                float y = radius_ * cosf(phi);
                float z = radius_ * sinf(phi) * sinf(theta);

                // UV座標（equirectangularに対応）
                vertices_.push_back({
                    { x, y, z },
                    { u, v }
                    });
            }
        }

        // インデックス生成
        for (unsigned int lat = 0; lat < latitude_segments_; ++lat) {
            for (unsigned int lon = 0; lon < longitude_segments_; ++lon) {
                unsigned int i0 = lat * (longitude_segments_ + 1) + lon;
                unsigned int i1 = i0 + longitude_segments_ + 1;

                indices_.push_back(i0);
                indices_.push_back(i1);
                indices_.push_back(i0 + 1);

                indices_.push_back(i0 + 1);
                indices_.push_back(i1);
                indices_.push_back(i1 + 1);
            }
        }
    }
    //バッファ生成
    {
        HRESULT hr{ S_OK };
        // 頂点バッファ作成
        {
            D3D11_BUFFER_DESC vb_desc{};
            vb_desc.Usage = D3D11_USAGE_DEFAULT;
            vb_desc.ByteWidth = static_cast<UINT>(sizeof(SkyVertex) * vertices_.size());
            vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vb_desc.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA vb_data{};
            vb_data.pSysMem = vertices_.data();

            hr = device->CreateBuffer(&vb_desc, &vb_data, vertex_buffer_.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }

        // インデックスバッファ作成
        {
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
        }

        //定数バッファ生成
        {
            D3D11_BUFFER_DESC cb_desc{};
            cb_desc.Usage = D3D11_USAGE_DEFAULT;
            cb_desc.ByteWidth = (sizeof(SkyAtmosphereCB)+15)/16*16;
            cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            hr = device->CreateBuffer(&cb_desc, nullptr, rayleigh_constant_buffer_.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        }
    }

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
    sky_flag_ = false;
    comp_mng_.ForEach<ComponentSkyAtmosphere>([&](uint32_t entity_id, ComponentSkyAtmosphere sky_atmosphere) {
        {
            sky_flag_ = true;

            ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

            RenderState render_state(Graphics::Instance().GetDevice());
            // 深度・カリング設定（球の内側を描画）
            render_state.GetDepthStencilState(DepthState::no_test_no_write);
            render_state.GetRasterizerState(RasterizerState::solid_cull_none);
            render_state.GetBlendState(BlendState::transparency);

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

            //定数バッファの設定
            sky_atmosphere_constant.rayleigh_scale_height = sky_atmosphere.rayleigh_scale_height;
            sky_atmosphere_constant.mie_scale_height = sky_atmosphere.mie_scale_height;
            sky_atmosphere_constant.ozone_scale_half_width = sky_atmosphere.ozone_scale_half_width;
            sky_atmosphere_constant.ozone_center_height = sky_atmosphere.ozone_center_height;
            sky_atmosphere_constant.atmosphere_height = sky_atmosphere.atmosphere_height;
            sky_atmosphere_constant.sun_distance = sky_atmosphere.sun_distance;
            sky_atmosphere_constant.earth_height = sky_atmosphere.earth_height;
            sky_atmosphere_constant.max_sample = sky_atmosphere.max_sample;
            sky_atmosphere_constant.height = comp_mng_.TryGetByEntity<ComponentPosition>(entity_id)->value.y * 1e3f;
            SkyAtmosphereCB data = sky_atmosphere_constant;
            context->UpdateSubresource(rayleigh_constant_buffer_.Get(), 0, 0, &data, 0, 0);
            Graphics::Instance().SetConstantBuffer(
                static_cast<int>(ConstantBufferSlot::kSkyAtmosphere),
                1,
                rayleigh_constant_buffer_.GetAddressOf());

            // 描画呼び出し
            context->DrawIndexed(index_count_, 0, 0);

            Graphics::Instance().ClearConstantBuffers(static_cast<int>(ConstantBufferSlot::kSkyAtmosphere), 1);
        }

        });
}

