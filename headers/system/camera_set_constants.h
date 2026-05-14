#pragma once
#include"i_render_system.h"
#include<d3d11.h>
#include<wrl.h>
#include<DirectXMath.h>

//前方宣言
class ComponentManager;

//レンダーマネージャの先頭でカメラの定数バッファを更新するシステム
//カメラのコンポーネント化、複数化に備えて、カメラの定数バッファを更新するシステムを分けておく
class CameraSetConstants 
{
public:
    CameraSetConstants(ComponentManager& comp_mng);

    void SetBuffer(ID3D11DeviceContext* context);

	DirectX::XMFLOAT4 GetCameraPosition(){
		return camera_position_;
    }

private:
    ComponentManager& comp_mng_;

    DirectX::XMFLOAT4 camera_position_{ 0.0f, 0.0f, -10.0f, 0.0f };

	struct CameraConstant 
	{
		DirectX::XMFLOAT4 camera_position{ 0.0f, 0.0f, -10.0f,0.0f };
		DirectX::XMFLOAT4 camera_direction;
		DirectX::XMFLOAT4 camera_clip_distance;//x:near,y:far,z:near * far,w:far-near
		DirectX::XMFLOAT4X4 view_transform;
		DirectX::XMFLOAT4X4 projection_transform;
		DirectX::XMFLOAT4X4 view_projection_transform;
		DirectX::XMFLOAT4X4 inverse_view_transform;
		DirectX::XMFLOAT4X4 inverse_projection_transform;
		DirectX::XMFLOAT4X4 inverse_view_projection_transform;
		DirectX::XMFLOAT4X4 previous_view_projection_transform;
	};

    Microsoft::WRL::ComPtr<ID3D11Buffer>camera_buffer_ = nullptr;
};