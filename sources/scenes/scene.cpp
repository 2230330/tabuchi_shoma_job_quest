#include"../../headers/scene/scene.h"

#include"../../headers/misc.h"
#include"../../headers/graphics.h"
#include"../../external/imgui/imgui.h"

Scene::Scene(const HWND hwnd)
	:hwnd(hwnd)
{
	HRESULT hr{ S_OK };

	//	定数バッファの生成
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
		{
			buffer_desc.ByteWidth = (sizeof(scene_constants)+15)/16*16;
			hr = Graphics::Instance().GetDevice()->CreateBuffer(&buffer_desc, nullptr, scene_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		}
	}
}

bool Scene::Initialize()
{
	return InitializeCore();
}

bool Scene::Uninitialize()
{
	return UninitializeCore();
}

void Scene::Update(float elapsed_time)
{
	timer += elapsed_time;

	previous_view_projection = view_projection;	//	古い行列を保存

		// マウス操作
#if USE_IMGUI
		if (!ImGui::IsAnyWindowHovered())
#endif // USE_IMGUI


	UpdateCore(elapsed_time);
}

void Scene::Render(float elapsed_time)
{
	//	バックバッファクリア
	Graphics::Instance().ViewClear(0.2f,0.2f,0.2f,0.0f);
	Graphics::Instance().SetRenderTargets();

	// ビューポートの設定
	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = Graphics::Instance().GetScreenWidth();
	viewport.Height = Graphics::Instance().GetScreenHeight();
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	Graphics::Instance().GetDeviceContext()->RSSetViewports(1, &viewport);

	RenderCore(elapsed_time);
}

void Scene::DrawGui()
{
	static bool prev_z = false;
	bool now_z = (::GetAsyncKeyState('Z') & 0x8000) != 0;

	if (now_z&&!prev_z)
	{
		imgui_draw_flag_ = !imgui_draw_flag_;
	}
	if (imgui_draw_flag_)
	{
		DrawImguiCore();
	}

	prev_z = now_z;
}

void Scene::SetSceneConstant(
	int start_slot ,
	bool is_update_resource,
	DirectX::XMFLOAT4 directional_light,
	DirectX::XMFLOAT2 viewport_size)
{
	if (is_update_resource)
	{
		scene_constants scene{};
		scene.options.x = static_cast<float>(cursor_position.x);
		scene.options.y = static_cast<float>(cursor_position.y);
		scene.options.z = timer;
		scene.options.w = flag;
		scene.z_buffer_parameteres.y = far_clip_distance / near_clip_distance;
		scene.z_buffer_parameteres.x = 1.0f - scene.z_buffer_parameteres.y;
		scene.z_buffer_parameteres.z = scene.z_buffer_parameteres.x / far_clip_distance;
		scene.z_buffer_parameteres.w = scene.z_buffer_parameteres.y / far_clip_distance;
		scene.viewport_size.x = viewport_size.x;
		scene.viewport_size.y = viewport_size.y;
		scene.viewport_size.z = 1.0f / viewport_size.x;
		scene.viewport_size.w = 1.0f / viewport_size.y;

		//画面上の太陽1の計算
		DirectX::XMVECTOR sun_dir =DirectX::XMLoadFloat4(&directional_light);
		sun_dir = DirectX::XMVector3Normalize(DirectX::XMVectorScale(sun_dir,-1.0f));
        DirectX::XMVECTOR sun_pos = DirectX::XMVectorAdd(
			DirectX::XMLoadFloat3(&camera_position), DirectX::XMVectorScale(sun_dir, 1000.0f));
		DirectX::XMMATRIX VP = DirectX::XMLoadFloat4x4(&view_projection); 

		DirectX::XMVECTOR sun_clip4 = DirectX::XMVector4Transform(
			DirectX::XMVectorSetW(sun_pos, 1.0f),
			VP
		);

		float w = DirectX::XMVectorGetW(sun_clip4);
		if (w <= 0.0001f) {
			// カメラ後方 or 不正：画面計算しない
			sun_visible_ = 0.0f;
		}
		else {
			DirectX::XMVECTOR sun_ndc = DirectX::XMVectorScale(sun_clip4, 1.0f / w);
			float ndcX = DirectX::XMVectorGetX(sun_ndc);
			float ndcY = DirectX::XMVectorGetY(sun_ndc);

			sun_uv_.x = 0.5f + 0.5f * ndcX;
			sun_uv_.y = 0.5f - 0.5f * ndcY;
			sun_visible_ = w;
		}


		scene.sun_uv = sun_uv_;
        scene.sun_visible = sun_visible_;

		Graphics::Instance().GetDeviceContext()->UpdateSubresource(scene_constant_buffer.Get(), 0, 0, &scene, 0, 0);
	}
	Graphics::Instance().SetConstantBuffer(start_slot, 1, scene_constant_buffer.GetAddressOf());
}
