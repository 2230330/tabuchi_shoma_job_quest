#ifndef PART2_SCENE_CONSTANTS_H
#define PART2_SCENE_CONSTANTS_H

#include<DirectXMath.h>

struct SceneConstants
{
    DirectX::XMFLOAT4X4 view_projection;
    DirectX::XMFLOAT4   light_direction;
    DirectX::XMFLOAT4   camera_position;
    DirectX::XMFLOAT4   light_color;
    float               light_intensity;
    DirectX::XMFLOAT3   dummy;
};

#endif // !PART2_SCENE_CONSTANTS_H
