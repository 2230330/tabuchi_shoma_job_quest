#ifndef PART2_SCENE_CONSTANTS_H
#define PART2_SCENE_CONSTANTS_H

#include"camera.h"
#include"light.h"

#include<DirectXMath.h>

struct SceneConstants
{
    DirectX::XMFLOAT4X4 view_projection;
    DirectX::XMFLOAT4   light_direction;
    DirectX::XMFLOAT4   camera_position;
};

#endif // !PART2_SCENE_CONSTANTS_H
