#include"../../headers/system/camera_set_constants.h"

#include"../../headers/graphics.h"
#include"../../headers/misc.h"
#include"../../headers/component/component_manager.h"
#include"../../headers/constant_buffer_slot.h"

CameraSetConstants::CameraSetConstants(ComponentManager& comp_mng)
    :comp_mng_(comp_mng)
{
    HRESULT hr{ S_OK };

    {
        D3D11_BUFFER_DESC buffer_desc{};
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = 0;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;
        {
            buffer_desc.ByteWidth = (sizeof(ComponentCamera) + 15) / 16 * 16;
            hr = Graphics::Instance().GetDevice()->CreateBuffer(&buffer_desc, nullptr, camera_buffer_.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }
    }
}

//カメラの定数バッファを更新するシステム
void CameraSetConstants::SetBuffer(ID3D11DeviceContext* context)
{
    comp_mng_.ForEach<ComponentCamera>([this, context](uint32_t entity_id, ComponentCamera& camera)
        {
            if (camera.main_camera_flag_)
            {
                camera_position_ = camera.camera_position;

                CameraConstant constant{};
                constant.camera_position = camera.camera_position;
                constant.camera_direction = camera.camera_direction;
                constant.camera_clip_distance = camera.camera_clip_distance;
                constant.view_transform = camera.view_transform;
                constant.projection_transform = camera.projection_transform;
                constant.view_projection_transform = camera.view_projection_transform;
                constant.inverse_view_transform = camera.inverse_view_transform;
                constant.inverse_projection_transform = camera.inverse_projection_transform;
                constant.inverse_view_projection_transform = camera.inverse_view_projection_transform;
                constant.previous_view_projection_transform = camera.previous_view_projection_transform;

                context->UpdateSubresource(
                    camera_buffer_.Get(), 0, nullptr, &constant, 0, 0);
                Graphics::Instance().SetConstantBuffer(
                    ConstantBufferSlot::kCamera, 1, camera_buffer_.GetAddressOf());
            }
        });
}
