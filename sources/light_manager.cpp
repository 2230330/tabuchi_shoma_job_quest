#include"../headers/light_manager.h"

#include<string>

#include"../headers/graphics.h"
#include"../headers/misc.h"
#include"../external/imgui/imgui.h"

LightManager::LightManager()
{
    HRESULT hr{ S_OK };
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;

	//フォワードレンダリング用定数バッファ
	{
		buffer_desc.ByteWidth = sizeof(ForwardLightConstants);
		hr = Graphics::Instance().GetDevice()->CreateBuffer(&buffer_desc, nullptr,
			forward_light_constant_buffer_.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	//フォワードレンダリング用
	for (int i = 0; i < ForwardLightConstants::light_max; i++)
	{
		PointLight point_light;
		point_lights_.emplace_back(point_light);

		SpotLight spot_light;
		spot_lights_.emplace_back(spot_light);
	}
}

void LightManager::SetForwardLightConstant(int start_slot)
{
	ForwardLightConstants constant;
	constant.directional_light = direction_light_;
	constant.ambient_color = { 0.2f,.2f,.2f,1.f };
	for (auto& light : point_lights_)
	{
		constant.point_light[constant.light_count.y] = light;
		if (++constant.light_count.y == ForwardLightConstants::light_max)
		{
			break;
		}
	}
	for (auto& light : spot_lights_)
	{
		constant.spot_light[constant.light_count.z] = light;
		constant.spot_light[constant.light_count.z].inner_corn = cosf(light.inner_corn);
		constant.spot_light[constant.light_count.z].outer_corn = cosf(light.outer_corn);
		if (++constant.light_count.z == ForwardLightConstants::light_max)
			break;
	}

	Graphics::Instance().GetDeviceContext()->UpdateSubresource(forward_light_constant_buffer_.Get(), 0, 0, &constant,0,0);
	Graphics::Instance().SetConstantBuffer(start_slot, 1, forward_light_constant_buffer_.GetAddressOf());
}

void LightManager::SetDeferredLightConstant(int start_slot)
{
	DeferredLightContstants constant;

	//環境光
	{
		constant.lights.work_data[0] = ambient_color;
        constant.lights.work_data[3].w = light_kind_ambient_light;
		Graphics::Instance().GetDeviceContext()
			->UpdateSubresource(deferred_light_constant_buffer_.Get(), 0, 0, &constant, 0, 0);
        Graphics::Instance().SetConstantBuffer(start_slot, 1, deferred_light_constant_buffer_.GetAddressOf());
	}

    //ディレクションライト
	{
		constant.lights.work_data[0] = direction_light_.direction;
		constant.lights.work_data[1] = direction_light_.color;
		constant.lights.work_data[3].w = light_kind_derectional_light;

        constant.use_shadow = 1;
		constant.shadow_attenuation = 0.5f;
        constant.shadow_bias = 0.001f;

		//ライト方向から見た視線行列を生成
        DirectX::XMVECTOR light_pos = DirectX::XMLoadFloat4(&direction_light_.direction);
        light_pos = DirectX::XMVectorScale(light_pos, -100.f);//ライト位置を少し後ろに
		DirectX::XMMATRIX V=DirectX::XMMatrixLookAtLH(
			light_pos,
			DirectX::XMVectorZero(),
			DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f)
        );
		//シャドウマップに描画したい範囲の射影行列を生成
        DirectX::XMMATRIX P = DirectX::XMMatrixOrthographicLH(200.f, 200.f, 0.1f, 400.f);
		//ライトビュー行列を保存
		DirectX::XMStoreFloat4x4(&constant.light_view_projection, V * P);

        Graphics::Instance().GetDeviceContext()
			->UpdateSubresource(deferred_light_constant_buffer_.Get(), 0, 0, &constant, 0, 0);
        Graphics::Instance().SetConstantBuffer(start_slot, 1, deferred_light_constant_buffer_.GetAddressOf());

	}

}

void LightManager::DrawImgui()
{
	if (ImGui::Begin("light manager"))
	{
		//ディレクションライト
		if (ImGui::TreeNode("directional light"))
		{
			if (ImGui::SliderAngle("azimuth", &azimuth_, -180.0f, 180.0f));
			if (ImGui::SliderAngle("elevation", &elevation_, -89.0f, 89.0f));

			direction_light_.direction.x = cosf(elevation_) * cosf(azimuth_);
			direction_light_.direction.y = sinf(elevation_);
			direction_light_.direction.z = cosf(elevation_) * sinf(azimuth_);

			if (ImGui::SliderFloat("intensity", &direction_light_.color.w, 0.0f, 20.f));
			
			ImGui::TreePop();
		}
		//ポイントライト
		if (ImGui::TreeNode("point_lights"))
		{
			int size = point_lights_.size();
			for (int i = 0; i < size; i++)
			{
				PointLight& light = point_lights_[i];
				std::string name = "point_light" + std::to_string(i);
				if (ImGui::TreeNode(name.c_str()))
				{
					if (ImGui::InputFloat4("position", &light.position.x));
					if (ImGui::ColorEdit4("color", &light.color.x));
					if (ImGui::SliderFloat("range", &light.range, 0.0f, 20.f));
					if (ImGui::SliderFloat("intensity", &light.intensity, 0.0f, 10.f));

					ImGui::TreePop();
				}
				
			}
			ImGui::TreePop();
		}
		//スポットライト
		if (ImGui::TreeNode("spot light"))
		{
			int size = spot_lights_.size();
			for (int i = 0; i < size ; i++)
			{
				SpotLight& light = spot_lights_[i];
				std::string name = "spot light" + std::to_string(i);
				if (ImGui::TreeNode(name.c_str()))
				{
					if (ImGui::InputFloat4("position", &light.position.x));
					if (ImGui::SliderFloat4("direction", &light.direction.x,-10.f,10.f));
					if (ImGui::ColorEdit4("color", &light.color.x));
					if (ImGui::SliderFloat("inner_corn", &light.inner_corn,
						DirectX::XMConvertToRadians(0.f),DirectX::XMConvertToRadians(180)));
					if (ImGui::SliderFloat("outer_corn", &light.outer_corn,
						DirectX::XMConvertToRadians(0.f),DirectX::XMConvertToRadians(180)));

					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
	ImGui::End();
	}
}