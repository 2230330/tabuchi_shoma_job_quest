#ifndef PART2_CAMERA_H
#define PART2_CAMERA_H

#include<DirectXMath.h>

class Camera
{
public:
    Camera();

    //指定方向を向く
    void SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up);
    //パースペクティブ設定
    void SetPerspectiveFov(float fov_y, float aspect, float near_z, float far_z);
    //ビュー行列取得
    const DirectX::XMFLOAT4X4 GetView()const { return view_; }
    //プロジェクション行列取得
    const DirectX::XMFLOAT4X4 GetProjection()const { return projection_; }
    //視点取得
    const DirectX::XMFLOAT3& GetEye()const { return eye_; }
    //注視点取得
    const DirectX::XMFLOAT3& GetFocus()const { return focus_; }
    //上方向取得
    const DirectX::XMFLOAT3& GetUp()const { return up_; }
    //前方向取得
    const DirectX::XMFLOAT3& GetFront()const { return front_; }
    //右方向取得
    const DirectX::XMFLOAT3& GetRight()const { return right_; }

private:
    DirectX::XMFLOAT4X4    view_;
    DirectX::XMFLOAT4X4    projection_;

    DirectX::XMFLOAT3      eye_;
    DirectX::XMFLOAT3      focus_;

    DirectX::XMFLOAT3      up_;
    DirectX::XMFLOAT3      front_;
    DirectX::XMFLOAT3      right_;
};

#endif //!PART2_CAMERA_H
