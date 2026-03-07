#ifndef PART2_SCENE_H
#define PART2_SCENE_H

#include<DirectXMath.h>
#include<d3d11.h>
#include<wrl.h>

#include"../graphics.h"
#include"../light_manager.h"

struct scene_constants
{
	DirectX::XMFLOAT4 options;	//	xy : マウスの座標値, z : タイマー, w : フラグ
	DirectX::XMFLOAT4 z_buffer_parameteres;
	DirectX::XMFLOAT4 camera_position;
	DirectX::XMFLOAT4 camera_direction;
	DirectX::XMFLOAT4 camera_clip_distance;
	DirectX::XMFLOAT4 viewport_size;
	DirectX::XMFLOAT4X4 view_transform;
	DirectX::XMFLOAT4X4 projection_transform;
	DirectX::XMFLOAT4X4 view_projection_transform;
	DirectX::XMFLOAT4X4 inverse_view_transform;
	DirectX::XMFLOAT4X4 inverse_projection_transform;
	DirectX::XMFLOAT4X4 inverse_view_projection_transform;
	DirectX::XMFLOAT4X4 previous_view_projection_transform;
	DirectX::XMFLOAT2 sun_uv; //画面上の太陽位置
	float sun_visible; //カメラ前方にあるか
	float dummy;
};

class Scene
{
public:
    Scene()=delete;
	Scene(const Scene&) = delete;
	Scene(Scene&&) = delete;
	Scene(const HWND hwnd);
    virtual ~Scene() = default;

	bool Initialize();
	bool Uninitialize();
    //更新処理
    void Update(float elapsed_time);
    //描画処理
    void Render(float elapsed_time);
    //GUI描画処理
    void DrawGui();


	void SetWheel(float wheel) { this->wheel = wheel; }

    DirectX::XMFLOAT3 GetCameraPosition() const { return camera_position; }
    POINT GetCursorPosition() const { return cursor_position; }


protected:
	virtual bool InitializeCore() { return true; }
	virtual bool UninitializeCore() { return true; }
	virtual void UpdateCore(float elapsed_time){}
	virtual void RenderCore(float elapsed_time){}
	virtual void DrawImguiCore(){}

	void SetSceneConstant(
		int start_slot = 1,
		bool is_update_resource = true,
		DirectX::XMFLOAT4 directional_light = { 0,0,0,1 },
		DirectX::XMFLOAT2 viewport_size = DirectX::XMFLOAT2(Graphics::Instance().GetScreenWidth(), Graphics::Instance().GetScreenHeight()));

	//static constexpr DirectX::XMFLOAT4X4 coordinate_system_transforms[]
	//{
	//	{ -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },	// 0:RHS Y-UP
	//	{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },		// 1:LHS Y-UP
	//	{ -1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1 },	// 2:RHS Z-UP
	//	{ 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1 },		// 3:LHS Z-UP
	//};
	//static constexpr float gltf_scale_factor = 1.0f;		//	単位をメートルにする場合は0.01に指定する事




private:
	CONST HWND hwnd;

	bool imgui_draw_flag_ = true;

	//render_data* renderdata{ nullptr };
	POINT cursor_position{ 0, 0 };
	float free_move_speed_ = 8.0f;
	float free_move_boost_ = 4.0f;
	float timer{ 0.0f };
	bool flag{ false };
	float near_clip_distance{ 0.1f };
	float far_clip_distance{ 1000.0f };
	float fov_y{ DirectX::XMConvertToRadians(30) };

	DirectX::XMFLOAT3 camera_position{ 0.0f, 0.0f, -10.0f };
	DirectX::XMFLOAT3 camera_focus{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
	DirectX::XMFLOAT4X4 view_projection;
	DirectX::XMFLOAT4X4 previous_view_projection;
	float rotateX{ 0.0f };
	float rotateY{ 0.0f };
	float wheel{ 0 };
	float distance{ 20.0f };
	float min_distance{ 10.f };
	float max_distance{ 100.f };

	//sun_uv
    DirectX::XMFLOAT2 sun_uv_{ 0.0f,0.0f };//画面上の太陽位置
	int sun_visible_{ 0 };//カメラ前方にあるか
	

	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;
};
#endif // !PART2_SCENE_H
