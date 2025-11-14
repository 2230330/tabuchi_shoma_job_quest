#include"../../headers/system/cloud_render_system.h"

#include<algorithm>

#include"../../headers/graphics.h"
#include"../../headers/resource_manager.h"
#include"../../headers/constant_buffer_slot.h"
#include"../../headers/misc.h"

CloudRenderSystem::CloudRenderSystem(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();

    //頂点情報とインデックス情報の作成
    {

        ////正20面体ベース
        const float t = (1.0f + sqrtf(5.0f)) / 2.0f;

        std::vector<DirectX::XMFLOAT3> positions = {
            {-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
            { 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
            { t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1}
        };

        std::vector<unsigned int> faces = {
            0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
            1,5,9, 5,11,4,11,10,2,10,7,6,7,1,8,
            3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
            4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1
        };

        // 正規化して半径を掛ける
        for (auto& p : positions) {
            DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&p);
            v = DirectX::XMVectorScale(DirectX::XMVector3Normalize(v) , radius_);
            DirectX::XMStoreFloat3(&p, v);
        }

        int subdivisions = 1;
        // 細分化
        for (int i = 0; i < subdivisions; ++i) {
            std::vector<unsigned int> newFaces;
            std::map<std::pair<unsigned int, unsigned int>, unsigned int> midpointCache;
            auto getMidpoint = [&](UINT a, UINT b) -> unsigned int{
                auto key = std::minmax(a, b);
                if (midpointCache.count(key)) return midpointCache[key];
                DirectX::XMFLOAT3 pa = positions[a];
                DirectX::XMFLOAT3 pb = positions[b];
                DirectX::XMFLOAT3 mid = {
                    (pa.x + pb.x) * 0.5f,
                    (pa.y + pb.y) * 0.5f,
                    (pa.z + pb.z) * 0.5f
                };
                DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&mid);
                v = DirectX::XMVectorScale(DirectX::XMVector3Normalize(v) , radius_);
                DirectX::XMStoreFloat3(&mid, v);
                unsigned int index = (unsigned int)positions.size();
                positions.push_back(mid);
                midpointCache[key] = index;
                return index;
            };
            for (size_t j = 0; j < faces.size(); j += 3) {
                unsigned int a = faces[j];
                unsigned int b = faces[j + 1];
                unsigned int c = faces[j + 2];
                unsigned int ab = getMidpoint(a, b);
                unsigned int bc = getMidpoint(b, c);
                unsigned int ca = getMidpoint(c, a);
                newFaces.insert(newFaces.end(), { a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca });
            }
            faces.swap(newFaces);
        }
        // 頂点とインデックスを格納
        for (auto& p : positions) {
            float u = atan2f(p.z, p.x) / DirectX::XM_2PI + 0.5f;
            float v = 1.0f-(p.y / radius_ * 0.5f + 0.5f);
            vertices_.push_back({ p, {u, v} });
        }
        indices_ = faces;


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
            cb_desc.ByteWidth = sizeof(CloudRayMarchingConstants);
            cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cb_desc.CPUAccessFlags=0;
            cb_desc.MiscFlags = 0;
            hr = device->CreateBuffer(&cb_desc, nullptr, cloud_ray_marching_constant_buffer_.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        }

        //シェーダーリソースビュー
        {
            D3D11_TEXTURE2D_DESC texture2d_desc{};
            texture2d_desc.Width = Graphics::Instance().GetScreenWidth()/down_sample;
            texture2d_desc.Height = Graphics::Instance().GetScreenHeight()/down_sample;
            texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            texture2d_desc.MipLevels = 1;
            texture2d_desc.ArraySize = 1;
            texture2d_desc.SampleDesc.Count = 1;
            texture2d_desc.SampleDesc.Quality = 0;
            texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
            texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            texture2d_desc.CPUAccessFlags = 0;
            texture2d_desc.MiscFlags = 0;

            Microsoft::WRL::ComPtr<ID3D11Texture2D>color_buffer{};
            hr= device->CreateTexture2D(&texture2d_desc, NULL, color_buffer.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

            hr = device->CreateRenderTargetView(color_buffer.Get(), NULL, low_res_rtv.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

            hr = device->CreateShaderResourceView(color_buffer.Get(), NULL, low_res_srv.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }

        //低解像度ビューポート設定
        {
            vp.TopLeftX = 0;
            vp.TopLeftY = 0;
            vp.Width = Graphics::Instance().GetScreenWidth() / down_sample;
            vp.Height = Graphics::Instance().GetScreenHeight() / down_sample;
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
        }
    }

    // InputLayoutとシェーダーの読み込み（仮）
    D3D11_INPUT_ELEMENT_DESC input_element_desc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    //シェーダーの設定
    cloud_vs_ = ResourceManager::Instance().LoadVertexShader(device, L".\\resources\\shader\\cloud_dome_vs.cso",
        cloud_il_.ReleaseAndGetAddressOf(), input_element_desc, _countof(input_element_desc));
    cloud_ps_ = ResourceManager::Instance().LoadPixelShader(device, L".\\resources\\shader\\cloud_dome_ps.cso");

    //フルスクリーンクワッドの作成
    fullscreen_quad_ = std::make_unique<FullscreenQuad>(device);
}
void CloudRenderSystem::Render()
{
    comp_mng_.ForEach<ComponentCloudDome>([&](uint32_t entity_id, ComponentCloudDome& cloud)
        {
            ID3D11DeviceContext* context = Graphics::Instance().GetDeviceContext();

            {
                float color[4] = { 0,0,0,1 };
                context->ClearRenderTargetView(low_res_rtv.Get(),color);
                context->OMSetRenderTargets(1, low_res_rtv.GetAddressOf(), nullptr);
                
            }

            RenderState render_state(Graphics::Instance().GetDevice());
            // 深度・カリング設定（球の内側を描画）
            render_state.GetDepthStencilState(DepthState::no_test_no_write);
            render_state.GetRasterizerState(RasterizerState::solid_cull_none);
            render_state.GetSamplerState(SamplerState::linear_clamp);

            // シェーダー設定
            context->VSSetShader(cloud_vs_.Get(), nullptr, 0);
            context->PSSetShader(cloud_ps_.Get(), nullptr, 0);
            context->IASetInputLayout(cloud_il_.Get());

            // 頂点・インデックスバッファ設定
            UINT stride = sizeof(SkyVertex);
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, vertex_buffer_.GetAddressOf(), &stride, &offset);
            context->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            //// テクスチャ設定（必要なら）
            auto* texture = comp_mng_.TryGetByEntity<ComponentTexture>(entity_id);
            if (texture)
            {
                texture->texture = low_res_srv;
            }


            //定数バッファの設定
        // 定数バッファ更新 (Map/Unmap)
            UpdateConstants(cloud);
            context->UpdateSubresource(
                cloud_ray_marching_constant_buffer_.Get(), 0, nullptr, &cloud_ray_marching_constant_,0, 0);
            Graphics::Instance().SetConstantBuffer(
                static_cast<int>(ConstantBufferSlot::kCloudDome),
                1,
                cloud_ray_marching_constant_buffer_.GetAddressOf());

            // 描画呼び出し
            context->DrawIndexed(index_count_, 0, 0);

            Graphics::Instance().ClearConstantBuffers(static_cast<int>(ConstantBufferSlot::kCloudDome),1);

            //元のレンダーターゲットに戻して合算
            Graphics::Instance().SetRenderTargets();
            //context->PSSetShaderResources(0, 1, low_res_srv.GetAddressOf());
            //render_state.GetSamplerState(SamplerState::linear_clamp);
            //render_state.GetBlendState(BlendState::additive);
            //context->Draw(6, 0);
            render_state.GetSamplerState(SamplerState::linear_clamp);
            fullscreen_quad_->blit(context, low_res_srv.GetAddressOf(), 0, 1);
        });
}

void CloudRenderSystem::UpdateConstants(const ComponentCloudDome& cloud)
{
    cloud_ray_marching_constant_.iteration = cloud.iteration;
    cloud_ray_marching_constant_.intensity = cloud.intensity;
    cloud_ray_marching_constant_.fog_scale = cloud.fog_scale;
    cloud_ray_marching_constant_.step_size = cloud.step_size;

    cloud_ray_marching_constant_.max_distance = cloud.max_distance;
    cloud_ray_marching_constant_.noise_intensity = cloud.noise_intensity;
    cloud_ray_marching_constant_.noise_threshold = cloud.noise_threshold;
    cloud_ray_marching_constant_.noise_seed = cloud.noise_seed;

    cloud_ray_marching_constant_.alpha_scale = cloud.alpha_scale;
    cloud_ray_marching_constant_.light_scatter_strength = cloud.light_scatter_strength;
    cloud_ray_marching_constant_.base_brightness = cloud.base_brightness;

    cloud_ray_marching_constant_.wind_direction = cloud.wind_direction;

    cloud_ray_marching_constant_.cloud_base = cloud.cloud_base;
    cloud_ray_marching_constant_.cloud_top = cloud.cloud_top;
}
