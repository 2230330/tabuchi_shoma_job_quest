#include"../../headers/system/update_camera_system.h"

#include<algorithm>

#include"../../headers/graphics.h"
#include"../../headers/component/component_manager.h"

CameraUpdateSystem::CameraUpdateSystem(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
}

void CameraUpdateSystem::Update(float elapsed_time)
{
	comp_mng_.ForEach<ComponentCamera>([this,elapsed_time](uint32_t entity_id, ComponentCamera& camera)
		{
			float screen_w = Graphics::Instance().GetScreenWidth();
			float screen_h = Graphics::Instance().GetScreenHeight();
			// 視線行列を生成
			DirectX::XMMATRIX V;
			{
				DirectX::XMVECTOR up{ DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) };

				// マウス操作
				{
					POINT cursor;
					RECT rc;
					::GetCursorPos(&cursor);
					::ScreenToClient(Graphics::Instance().GetWindowHandle(), &cursor);
					GetClientRect(Graphics::Instance().GetWindowHandle(), &rc);
					UINT screenW = rc.right - rc.left;
					UINT screenH = rc.bottom - rc.top;
					POINT old_cursor_position;
					old_cursor_position.x = cursor_position_.x;
					old_cursor_position.y = cursor_position_.y;
					cursor_position_.x = static_cast<LONG>(
						(static_cast<float>(cursor.x) / screen_w) * static_cast<float>(screenW));
					cursor_position_.y = static_cast<LONG>(
						(static_cast<float>(cursor.y) / screen_h) * static_cast<float>(screenH));

					float moveX = (cursor_position_.x - old_cursor_position.x) * 0.02f;
					float moveY = (cursor_position_.y - old_cursor_position.y) * 0.02f;
					if (::GetAsyncKeyState(VK_RBUTTON) & 0x8000)
					{
						bool move_flag = false;
						//WASD移動
						float speed = free_move_speed_;
						if (::GetAsyncKeyState(VK_SHIFT) & 0x8000)
							speed *= free_move_boost_;

						float dt = elapsed_time;
						float move = speed * dt;

						// 今のカメラ向きから前/右/上ベクトルを作る
						float sx = ::sinf(rotateX), cx = ::cosf(rotateX);
						float sy = ::sinf(rotateY), cy = ::cosf(rotateY);

						DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(
							DirectX::XMVectorSet(-cx * sy, -sx, -cx * cy, 0.0f));

						DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);
						DirectX::XMVECTOR right = DirectX::XMVector3Normalize(
							DirectX::XMVector3Cross(up, forward));  // LH系でこの向きが自然

						DirectX::XMVECTOR delta = DirectX::XMVectorZero();

						if (::GetAsyncKeyState('W') & 0x8000) { delta = DirectX::XMVectorAdd(delta, forward); move_flag = true; }
						if (::GetAsyncKeyState('S') & 0x8000) { delta = DirectX::XMVectorSubtract(delta, forward); move_flag = true; }
						if (::GetAsyncKeyState('D') & 0x8000) { delta = DirectX::XMVectorAdd(delta, right); move_flag = true; }
						if (::GetAsyncKeyState('A') & 0x8000) { delta = DirectX::XMVectorSubtract(delta, right); move_flag = true; }

						// UEっぽく上下移動も
						if (::GetAsyncKeyState('E') & 0x8000) delta = DirectX::XMVectorAdd(delta, up);
						if (::GetAsyncKeyState('Q') & 0x8000) delta = DirectX::XMVectorSubtract(delta, up);

						// 斜め移動が速くならないよう正規化
						DirectX::XMVECTOR len = DirectX::XMVector3Length(delta);
						float l = DirectX::XMVectorGetX(len);
						if (l > 0.0001f)
						{
							delta = DirectX::XMVectorScale(DirectX::XMVector3Normalize(delta), move);

							DirectX::XMVECTOR focus = DirectX::XMLoadFloat3(&camera_focus);
							focus = DirectX::XMVectorAdd(focus, delta);
							DirectX::XMStoreFloat3(&camera_focus, focus);
						}

						float rotate_speed = 0.5f;
						if (move_flag)
						{
							rotate_speed *= 0.1f;
						}
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
						V = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat4(&camera.camera_position),
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

					wheel_ = Graphics::Instance().GetWheel();
					if (wheel_ != 0.0)	// ズーム
					{
						distance -= static_cast<float>(wheel_) * distance * 0.001f;
						distance = (std::max)((std::min)(distance, max_distance), min_distance);
						wheel_ = 0.;
					}
				}
				float sx = ::sinf(rotateX), cx = ::cosf(rotateX);
				float sy = ::sinf(rotateY), cy = ::cosf(rotateY);
				DirectX::XMVECTOR Focus = DirectX::XMLoadFloat3(&camera_focus);
				DirectX::XMVECTOR Front = DirectX::XMVectorSet(-cx * sy, -sx, -cx * cy, 0.0f);
				DirectX::XMVECTOR Distance = DirectX::XMVectorSet(distance, distance, distance, 0.0f);
				Front = DirectX::XMVectorMultiply(Front, Distance);
				DirectX::XMVECTOR Eye = DirectX::XMVectorSubtract(Focus, Front);
				DirectX::XMStoreFloat4(&camera.camera_position, Eye);
				V = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat4(&camera.camera_position),
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


			camera.camera_position.w = 1.f;
			camera.camera_direction.x = camera_focus.x - camera.camera_position.x;
			camera.camera_direction.y = camera_focus.y - camera.camera_position.y;
			camera.camera_direction.z = camera_focus.z - camera.camera_position.z;
			float d = sqrtf(camera.camera_direction.x * camera.camera_direction.x + camera.camera_direction.y * camera.camera_direction.y + camera.camera_direction.z * camera.camera_direction.z);
			camera.camera_direction.x /= d;
			camera.camera_direction.y /= d;
			camera.camera_direction.z /= d;
			camera.camera_direction.w = 1;			
			camera.camera_clip_distance.x = near_clip_distance;
			camera.camera_clip_distance.y = far_clip_distance;
			camera.camera_clip_distance.z = near_clip_distance * far_clip_distance;
			camera.camera_clip_distance.w = far_clip_distance - near_clip_distance;
			DirectX::XMStoreFloat4x4(&camera.view_transform, V);
			DirectX::XMStoreFloat4x4(&camera.projection_transform, P);
			DirectX::XMStoreFloat4x4(&camera.view_projection_transform, V * P);
			DirectX::XMStoreFloat4x4(&camera.inverse_view_transform, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&camera.view_transform)));
			DirectX::XMStoreFloat4x4(&camera.inverse_projection_transform, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&camera.projection_transform)));
			DirectX::XMStoreFloat4x4(&camera.inverse_view_projection_transform, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&camera.view_projection_transform)));

});
}
