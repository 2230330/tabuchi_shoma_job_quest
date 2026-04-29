#pragma once
#include"i_update_system.h"
#include"../component/component_manager.h"

//カメラの更新を行うシステム
//このシステムは、ComponentCameraを持つエンティティが存在する場合に、
//カメラの位置や向きを更新します。
class CameraUpdateSystem :public IUpdateSystem
{
public:
    CameraUpdateSystem(ComponentManager& comp_mng);

    void Update(float elapsed_time)override;

private:
    ComponentManager& comp_mng_;

    POINT cursor_position_{ 0,0 };
    float wheel_ = 0.0f;

    float free_move_speed_ = 8.0f;
    float free_move_boost_ = 4.0f;
    float rotateX = 0.0f;
    float rotateY = 0.0f;
    float distance = 10.0f;
    float min_distance{ 5.f };
    float max_distance{ 100.f };
    float near_clip_distance{ 0.1f };
    float far_clip_distance{ 1000.0f };
    float fov_y{ DirectX::XMConvertToRadians(30) };
    //カメラの初期位置
    DirectX::XMFLOAT3 camera_focus{ 0.0f, 0.0f, 0.0f };
};