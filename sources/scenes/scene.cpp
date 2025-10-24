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
			buffer_desc.ByteWidth = sizeof(scene_constants);
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

	float screen_w = Graphics::Instance().GetScreenWidth();
	float screen_h = Graphics::Instance().GetScreenHeight();
	// 視線行列を生成
	DirectX::XMMATRIX V;
	{
		DirectX::XMVECTOR up{ DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) };

		// マウス操作
#if USE_IMGUI
		if (!ImGui::IsAnyWindowHovered())
#endif // USE_IMGUI
		{
			POINT cursor;
			RECT rc;
			::GetCursorPos(&cursor);
			::ScreenToClient(hwnd, &cursor);
			GetClientRect(hwnd, &rc);
			UINT screenW = rc.right - rc.left;
			UINT screenH = rc.bottom - rc.top;
			POINT old_cursor_position;
			old_cursor_position.x = cursor_position.x;
			old_cursor_position.y = cursor_position.y;
			cursor_position.x = (LONG)(cursor.x / screen_w * static_cast<float>(screenW));
			cursor_position.y = (LONG)(cursor.y / screen_h * static_cast<float>(screenH));

			float moveX = (cursor_position.x - old_cursor_position.x) * 0.02f;
			float moveY = (cursor_position.y - old_cursor_position.y) * 0.02f;
			if (::GetAsyncKeyState(VK_RBUTTON) & 0x8000)
			{
				// Y軸回転
				rotateY += moveX * 0.5f;
				if (rotateY > DirectX::XM_PI)
					rotateY -= DirectX::XM_2PI;
				else if (rotateY < -DirectX::XM_PI)
					rotateY += DirectX::XM_2PI;
				// X軸回転
				rotateX += moveY * 0.5f;
				if (rotateX > DirectX::XMConvertToRadians(89.9f))
					rotateX = DirectX::XMConvertToRadians(89.9f);
				else if (rotateX < -DirectX::XMConvertToRadians(89.9f))
					rotateX = -DirectX::XMConvertToRadians(89.9f);
			}
			else if (::GetAsyncKeyState(VK_MBUTTON) & 0x8000)
			{
				V = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&camera_position),
					DirectX::XMLoadFloat3(&camera_focus),
					up);
				DirectX::XMFLOAT4X4 W;
				DirectX::XMStoreFloat4x4(&W, DirectX::XMMatrixInverse(nullptr, V));
				// 平行移動
				float s = distance * 0.035f;
				float x = moveX * s;
				float y = moveY * s;
				camera_focus.x -= W._11 * x;
				camera_focus.y -= W._12 * x;
				camera_focus.z -= W._13 * x;

				camera_focus.x += W._21 * y;
				camera_focus.y += W._22 * y;
				camera_focus.z += W._23 * y;
			}
			if (wheel != 0)	// ズーム
			{
				distance -= static_cast<float>(wheel) * distance * 0.001f;
				distance = max(min(distance,max_distance), min_distance);
				wheel = 0;
			}
		}
		float sx = ::sinf(rotateX), cx = ::cosf(rotateX);
		float sy = ::sinf(rotateY), cy = ::cosf(rotateY);
		DirectX::XMVECTOR Focus = DirectX::XMLoadFloat3(&camera_focus);
		DirectX::XMVECTOR Front = DirectX::XMVectorSet(-cx * sy, -sx, -cx * cy, 0.0f);
		DirectX::XMVECTOR Distance = DirectX::XMVectorSet(distance, distance, distance, 0.0f);
		Front = DirectX::XMVectorMultiply(Front, Distance);
		DirectX::XMVECTOR Eye = DirectX::XMVectorSubtract(Focus, Front);
		DirectX::XMStoreFloat3(&camera_position, Eye);
		V = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&camera_position),
			DirectX::XMLoadFloat3(&camera_focus),
			up);
	}

	// 射影行列を生成
	DirectX::XMMATRIX P;
	{
		D3D11_VIEWPORT viewport;
		UINT num_viewports{ 1 };
		Graphics::Instance().GetDeviceContext()->RSGetViewports(&num_viewports, &viewport);
		float aspect_ratio{ (screen_w) / (screen_h) };
		P = DirectX::XMMatrixPerspectiveFovLH(fov_y,
			aspect_ratio,
			near_clip_distance,
			far_clip_distance);
	}

	DirectX::XMStoreFloat4x4(&view, V);
	DirectX::XMStoreFloat4x4(&projection, P);
	DirectX::XMStoreFloat4x4(&view_projection, V * P);

	UpdateCore(elapsed_time);
}

void Scene::Render(float elapsed_time)
{
	//	バックバッファクリア
	Graphics::Instance().ViewClear(0.2f,0.2f,0.2f,1.0f);
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

void Scene::SetSceneConstant(int start_slot,DirectX::XMFLOAT2 viewport_size ,bool is_update_resource)
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
		scene.camera_position.x = camera_position.x;
		scene.camera_position.y = camera_position.y;
		scene.camera_position.z = camera_position.z;
		scene.camera_position.w = 1;
		scene.camera_direction.x = camera_focus.x - camera_position.x;
		scene.camera_direction.y = camera_focus.y - camera_position.y;
		scene.camera_direction.z = camera_focus.z - camera_position.z;
		float d = sqrtf(scene.camera_direction.x * scene.camera_direction.x + scene.camera_direction.y * scene.camera_direction.y + scene.camera_direction.z * scene.camera_direction.z);
		scene.camera_direction.x /= d;
		scene.camera_direction.y /= d;
		scene.camera_direction.z /= d;
		scene.camera_direction.w = 1;
		scene.camera_clip_distance.x = near_clip_distance;
		scene.camera_clip_distance.y = far_clip_distance;
		scene.camera_clip_distance.z = near_clip_distance * far_clip_distance;
		scene.camera_clip_distance.w = far_clip_distance - near_clip_distance;
		scene.viewport_size.x = viewport_size.x;
		scene.viewport_size.y = viewport_size.y;
		scene.viewport_size.z = 1.0f / viewport_size.x;
		scene.viewport_size.w = 1.0f / viewport_size.y;
		scene.view_transform = view;
		scene.projection_transform = projection;
		scene.view_projection_transform = view_projection;
		scene.previous_view_projection_transform = previous_view_projection;

		DirectX::XMStoreFloat4x4(&scene.inverse_view_transform, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&view)));
		DirectX::XMStoreFloat4x4(&scene.inverse_projection_transform, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&projection)));
		DirectX::XMStoreFloat4x4(&scene.inverse_view_projection_transform, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&view_projection)));

		Graphics::Instance().GetDeviceContext()->UpdateSubresource(scene_constant_buffer.Get(), 0, 0, &scene, 0, 0);
	}
	Graphics::Instance().SetConstantBuffer(start_slot, 1, scene_constant_buffer.GetAddressOf());
}
