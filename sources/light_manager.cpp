#include"../headers/light_manager.h"

#include<string>
#include<cmath>

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
	//ディファードレンダリング用定数バッファ
	{
		buffer_desc.ByteWidth = sizeof(DeferredLightContstants);
		hr = Graphics::Instance().GetDevice()->CreateBuffer(&buffer_desc, nullptr,
			deferred_light_constant_buffer_.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	////フォワードレンダリング用
	//for (int i = 0; i < forward_light_max; i++)
	//{
	//	PointLight point_light;
	//	point_lights_.emplace_back(point_light);

	//	SpotLight spot_light;
	//	spot_lights_.emplace_back(spot_light);
	//}

}


void LightManager::SetForwardLightConstant(int start_slot)
{

	ForwardLightConstants constant;
	constant.directional_light = direction_light_;
	constant.ambient_color = ambient_color_;
    constant.light_view_position = light_view_projection_;
    constant.inverse_light_view_position = inverse_light_view_projection_;
    constant.light_orthographic_size = { shadow_map_size_,shadow_map_size_ };
    constant.light_depth_range = { shadow_near_plane_,shadow_far_plane_ };

	for (auto& light : point_lights_)
	{
		constant.point_light[constant.light_count.y] = light;
		if (++constant.light_count.y == forward_light_max)
		{
			break;
		}
	}
	for (auto& light : spot_lights_)
	{
		constant.spot_light[constant.light_count.z] = light;
		constant.spot_light[constant.light_count.z].inner_corn = cosf(light.inner_corn);
		constant.spot_light[constant.light_count.z].outer_corn = cosf(light.outer_corn);
		if (++constant.light_count.z == forward_light_max)
			break;
	}

	Graphics::Instance().GetDeviceContext()->UpdateSubresource(forward_light_constant_buffer_.Get(), 0, 0, &constant,0,0);
	Graphics::Instance().SetConstantBuffer(start_slot, 1, forward_light_constant_buffer_.GetAddressOf());
}

void LightManager::BindDeferredLightConstant(int start_slot, UINT index)
{
	auto* ctx = Graphics::Instance().GetDeviceContext();

	ctx->UpdateSubresource(deferred_light_constant_buffer_.Get(), 0, 0, &deferred_lights_[index], 0, 0);
	Graphics::Instance().SetConstantBuffer(start_slot, 1, deferred_light_constant_buffer_.GetAddressOf());

}


void LightManager::DrawImgui()
{
	if (ImGui::Begin("light manager"))
	{
		if (ImGui::TreeNode("ambient_color"))
		{
			if (ImGui::ColorEdit4("ambient", &ambient_color_.x));
			ImGui::TreePop();
		}

		//ディレクションライト
		if (ImGui::TreeNode("directional light"))
		{
			if (ImGui::SliderAngle("azimuth", &azimuth_, -180.0f, 180.0f));
			if (ImGui::SliderAngle("elevation", &elevation_, -90.0f, 90.0f));
			if (ImGui::SliderFloat("intensity", &direction_light_.color.w, 0.0f, 20.f));

			if(ImGui::Button("moove_", ImVec2(64, 32)))
			{
				moove_light_ = !moove_light_;
			}
			if (moove_light_)
			{
				if (ImGui::DragFloat("day_length_seconds", &day_length_seconds_,1.0f));
			}
			
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
					if (ImGui::DragFloat4("position", &light.position.x));
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
					if (ImGui::DragFloat4("position", &light.position.x));
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

void LightManager::Update(float elapsed_time)
{

	const float kMinEl = -DirectX::XM_PIDIV2 + 0.001f; // ちょい余裕（真上/真下の特異点回避）
	const float kMaxEl = DirectX::XM_PIDIV2 - 0.001f;
	if (elevation_ < kMinEl) elevation_ = kMinEl;
	if (elevation_ > kMaxEl) elevation_ = kMaxEl;

	// azimuth_: [-180°, +180°] にラップ（正規化）
	// a = fmod(a + pi, 2pi) -> [0,2pi) -> [-pi,pi)
	azimuth_ = std::fmod(azimuth_ + DirectX::XM_PI, DirectX::XM_2PI);
	if (azimuth_ < 0.0f) azimuth_ += DirectX::XM_2PI;
	azimuth_ -= DirectX::XM_PI;


	//ディレクションライトを太陽の様に動かす
	if (moove_light_)
	{
		if (day_length_seconds_ >= 0.01f||day_length_seconds_<=-0.01f)
		{
			time_of_day += elapsed_time / day_length_seconds_;
			if (time_of_day > 1.0f)
				time_of_day -= 1.0f;
		}


		float theta = time_of_day * DirectX::XM_2PI;
		//太陽の軌道円を作る
		DirectX::XMVECTOR pos = DirectX::XMVectorSet(cosf(theta), 0.0f, sinf(theta), 0.0f);

		//軌道を傾ける,X軸回転で軌道面を傾ける
		DirectX::XMMATRIX tilt = DirectX::XMMatrixRotationX(sun_tilt_);
		pos = DirectX::XMVector3TransformNormal(pos, tilt);

		DirectX::XMVECTOR dir = DirectX::XMVectorNegate(pos);
		dir = DirectX::XMVector3Normalize(dir);
		DirectX::XMFLOAT3 d;
		DirectX::XMStoreFloat3(&d, dir);

		direction_light_.direction.x = d.x;
		direction_light_.direction.y = d.y;
		direction_light_.direction.z= d.z;

    }
	else
	{ 

		direction_light_.direction.x = cosf(elevation_) * cosf(azimuth_);
		direction_light_.direction.y = sinf(elevation_);
		direction_light_.direction.z = cosf(elevation_) * sinf(azimuth_);
	}

	
	//ライト方向から見た視線行列を生成

	DirectX::XMVECTOR light_dir = DirectX::XMVector3Normalize(DirectX::XMLoadFloat4(&direction_light_.direction));

	// 影中心からライト方向に引いた位置にライトを置く
	DirectX::XMVECTOR light_pos = DirectX::XMVectorScale(light_dir, shadow_distance_);

	// 注視点は中心
	DirectX::XMVECTOR center = DirectX::XMVectorSet(0,0,0,0);
	DirectX::XMVECTOR target = center;

	// up が light_dir と平行に近い場合の対策
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f);
	if (fabs(DirectX::XMVectorGetX(DirectX::XMVector3Dot(light_dir, up))) > 0.95f)
		up = DirectX::XMVectorSet(0.f, 0.f, 1.f, 0.f);

	DirectX::XMMATRIX V = DirectX::XMMatrixLookAtLH(light_pos, target, up);
    DirectX::XMStoreFloat4x4(&light_view_, V);

	// 正射影（範囲は雲影を落としたい地面サイズ）
	float w = shadow_map_size_;
	float h = shadow_map_size_;

	DirectX::XMMATRIX P = DirectX::XMMatrixOrthographicLH(w, h, shadow_near_plane_, shadow_far_plane_);
    DirectX::XMStoreFloat4x4(&light_projection_, P);

	DirectX::XMMATRIX VP = V * P;
	DirectX::XMStoreFloat4x4(&light_view_projection_, VP);
	DirectX::XMStoreFloat4x4(&inverse_light_view_projection_, DirectX::XMMatrixInverse(nullptr, VP));

	BuildDeferredLights();
}
void LightManager::BuildDeferredLights()
{
	deferred_lights_.clear();

	// Ambient
	{
		//環境光の場合
		//work_data[0]=color
		//work_data[1]=dummy
		//work_data[2]=dummy
		//work_data[3]=xyz=dummy,w=ライト識別番号
		DeferredLightContstants l{};
		l.lights.work_data[0] = ambient_color_;
		l.lights.work_data[3].w = static_cast<float>(light_kind_ambient_light);

		deferred_lights_.emplace_back(l);

	}

	// Directional
	{
		//平行光源の場合
		//work_data[0]=direction
		//work_data[1]=color
		//work_data[2]=dummy
		//work_data[3]=xyz=dummy,w=ライト識別番号
		DeferredLightContstants l{};
		l.lights.work_data[0] = direction_light_.direction;
		l.lights.work_data[1] = direction_light_.color;
		l.lights.work_data[3].w = static_cast<float>(light_kind_derectional_light);
		l.use_shadow = 1;
		l.light_view_projection = light_view_projection_;

		deferred_lights_.emplace_back(l);
	}

	// Point
	for (auto& p : point_lights_)
	{
		//点光源
		//work_data[0]=position
		//work_data[1]=color
		//work_data[2]=x=range,yzw=dummy
		//work_data[3]=xyz=dummy,w=ライト識別番号
		DeferredLightContstants l{};
		l.lights.work_data[0] = p.position;
		l.lights.work_data[1] = p.color;
		l.lights.work_data[2].x = p.range;
		l.lights.work_data[3].w = static_cast<float>(light_kind_point_light);

		deferred_lights_.emplace_back(l);
	}

	// Spot
	for (auto& s : spot_lights_)
	{

		//スポットライトの場合
		//work_data[0]=position
		//work_data[1]=direction
		//work_data[2]=color
		//work_data[3]=x=range,y=inner_cone,z=outer_cone,w=ライト識別番号
		DeferredLightContstants l{};
		l.lights.work_data[0] = s.position;
		l.lights.work_data[1] = s.direction;
		l.lights.work_data[2] = s.color;
		l.lights.work_data[3] = { s.range,s.inner_corn,s.outer_corn,static_cast<float>(light_kind_spot_light) };

		deferred_lights_.emplace_back(l);
	}
}
