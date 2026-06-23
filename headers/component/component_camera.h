#pragma once
#include"i_component.h"
#include<DirectXMath.h>

struct ComponentCamera :public IComponent
{
	DirectX::XMFLOAT4 camera_position{ 0.0f, 0.0f, -10.0f,0.0f };
	DirectX::XMFLOAT4 camera_direction{ 0.f,0.f,0.f,0.f };
	DirectX::XMFLOAT4 camera_clip_distance;//x:near,y:far,z:near * far,w:far-near
	DirectX::XMFLOAT4X4 view_transform;
	DirectX::XMFLOAT4X4 projection_transform;
	DirectX::XMFLOAT4X4 view_projection_transform;
	DirectX::XMFLOAT4X4 inverse_view_transform;
	DirectX::XMFLOAT4X4 inverse_projection_transform;
	DirectX::XMFLOAT4X4 inverse_view_projection_transform;
	DirectX::XMFLOAT4X4 previous_view_projection_transform;
	bool main_camera_flag_ = false;
};