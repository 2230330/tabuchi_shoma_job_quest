#pragma once

#include<DirectXMath.h>
#include<array>

struct ComponentBoundingBox;

namespace FrustumHelper
{
    // フラスタム平面を正規化する関数
    inline void NormalizePlane(DirectX::XMFLOAT4& plane)
    {
        const float length = std::sqrt(
            plane.x * plane.x +
            plane.y * plane.y +
            plane.z * plane.z
        );

        if (length <= 0.0f)
        {
            return;
        }

        const float inv_length = 1.0f / length;

        plane.x *= inv_length;
        plane.y *= inv_length;
        plane.z *= inv_length;
        plane.w *= inv_length;
    }

    // ビュー射影行列からフラスタム平面を作成する関数
    inline void CreateFrustumPlanesFromMatrix(
        const DirectX::XMFLOAT4X4& m,
        std::array<DirectX::XMFLOAT4, 6>& planes)
    {

        // planes[0] : Left
        planes[0] =
        {
            m._11 + m._14,
            m._21 + m._24,
            m._31 + m._34,
            m._41 + m._44
        };

        // planes[1] : Right
        planes[1] =
        {
            -m._11 + m._14,
            -m._21 + m._24,
            -m._31 + m._34,
            -m._41 + m._44
        };

        // planes[2] : Bottom
        planes[2] =
        {
            m._12 + m._14,
            m._22 + m._24,
            m._32 + m._34,
            m._42 + m._44
        };

        // planes[3] : Top
        planes[3] =
        {
            -m._12 + m._14,
            -m._22 + m._24,
            -m._32 + m._34,
            -m._42 + m._44
        };

        // planes[4] : Near
        planes[4] =
        {
            m._13,
            m._23,
            m._33,
            m._43
        };

        // planes[5] : Far
        planes[5] =
        {
            -m._13 + m._14,
            -m._23 + m._24,
            -m._33 + m._34,
            -m._43 + m._44
        };

        for (auto& plane : planes)
        {
            NormalizePlane(plane);
        }
    }

    // バウンディングボックスが有効かどうかを判定する関数
    inline bool IsValidWorldBoundingBox(const ComponentBoundingBox& bbox)
    {
        return
            bbox.world_min.x <= bbox.world_max.x &&
            bbox.world_min.y <= bbox.world_max.y &&
            bbox.world_min.z <= bbox.world_max.z;
    }

    //AABBとフラスタムの判定
    inline bool IsAABBVisibleFromFrustumPlanes(
        const ComponentBoundingBox& bbox,
        const std::array<DirectX::XMFLOAT4, 6>& planes)
    {
        for (const auto& plane : planes)
        {
            const float px = plane.x >= 0.0f ? bbox.world_max.x : bbox.world_min.x;
            const float py = plane.y >= 0.0f ? bbox.world_max.y : bbox.world_min.y;
            const float pz = plane.z >= 0.0f ? bbox.world_max.z : bbox.world_min.z;

            const float distance =
                plane.x * px +
                plane.y * py +
                plane.z * pz +
                plane.w;

            if (distance < 0.0f)
            {
                return false;
            }
        }

        return true;
    }


}