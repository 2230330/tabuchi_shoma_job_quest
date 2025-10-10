#include"../headers/light_manager.h"
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
		point_light.position = { 0.f,0.f,0.f,0.f };
		point_light.color = { 0.f,0.f,0.f,0.f };
		point_light.range = 10.f;
		point_light.intensity = 1.0f;

		point_lights_.emplace_back(point_light);

		SpotLight spot_light;
		spot_light.position = { 0.f,0.f,0.f,0.f };
		spot_light.color = { 0.f,0.f,0.f,0.f };
		spot_light.range = 10.f;
		
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

void LightManager::DrawImgui()
{
	if (ImGui::Begin("light manager"))
	{
		if (ImGui::TreeNode("directional light"))
		{
			if (ImGui::SliderFloat4("direction", &direction_light_.direction.x, -1.f, 1.f));
			if (ImGui::ColorEdit4("color", &direction_light_.color.x));
			if (ImGui::SliderFloat("intensity", &direction_light_.intensity, 0.1f, 10.f));
			
			ImGui::TreePop();
		}

	ImGui::End();
	}
}