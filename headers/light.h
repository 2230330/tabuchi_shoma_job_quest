#ifndef PART2_LIGHT_H
#define PART2_LIGHT_H

#include<DirectXMath.h>

struct DirectionLight
{
    DirectX::XMFLOAT4    direction{ 1,1,-1,1 };
    DirectX::XMFLOAT4    color{ 1,1,1,1 };
    float intensity{ 1.0f };
};

class SceneLightManager
{
public:
    //ディレクションライト設定
    void SetDirectionLight(DirectionLight& light) { this->direction_light_ = light; }

    //ディレクションライト取得
    DirectionLight GetDirectionLight()const { return this->direction_light_; }

private:
    DirectionLight direction_light_;
};

#endif //!PART2_LIGHT_H