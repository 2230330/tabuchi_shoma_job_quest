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
void CameraSetConstants::SetBuffer(ID3D11DeviceContext* context,const DirectX::XMFLOAT4&light_direction)
{
    comp_mng_.ForEach<ComponentCamera>([this, context,light_direction]
	(uint32_t entity_id, ComponentCamera& camera)
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


				//画面上の太陽1の計算
				DirectX::XMVECTOR light_dir = DirectX::XMLoadFloat4(&light_direction);
				float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(light_dir));

				if (length > 0.00001f)
				{

					light_dir = DirectX::XMVector3Normalize(DirectX::XMVectorNegate(light_dir));

					DirectX::XMVECTOR camera_pos = DirectX::XMLoadFloat4(&constant.camera_position);

					DirectX::XMVECTOR sun_pos = DirectX::XMVectorAdd(
						camera_pos,
						DirectX::XMVectorScale(
							light_dir,
							1000.0f));

					DirectX::XMMATRIX VP = DirectX::XMLoadFloat4x4(&constant.view_projection_transform);

                    DirectX::XMVECTOR clip =
                        DirectX::XMVector4Transform(
                            DirectX::XMVectorSet(
                                DirectX::XMVectorGetX(sun_pos),
                                DirectX::XMVectorGetY(sun_pos),
                                DirectX::XMVectorGetZ(sun_pos),
                                1.0f
                            )
                            ,VP
                        );

                    float clip_x = DirectX::XMVectorGetX(clip);
                    float clip_y = DirectX::XMVectorGetY(clip);
                    float clip_z = DirectX::XMVectorGetZ(clip);
                    float clip_w = DirectX::XMVectorGetW(clip);

                    if (clip_w > 0.00001f)//背面以外なら、０は除算できないので
                    {
                        float inv_w = 1.0f / clip_w;

                        float ndc_x = clip_x * inv_w;
                        float ndc_y = clip_y * inv_w;
                        float ndc_z = clip_z * inv_w;

                        sun_uv_.x = 0.5f + (ndc_x * 0.5f);
                        sun_uv_.y = 0.5f - (ndc_x * 0.5f);

                        bool visible =
                            ndc_x >= -1.0f &&
                            ndc_x <= 1.0f &&
                            ndc_y >= -1.0f &&
                            ndc_y <= 1.0f;

                        sun_visible_ = visible ? 1.0f : 0.0f;

                        if (!visible)
                        {
                            sun_uv_.x = 0.5f;
                            sun_uv_.y = 0.5f;
                        }

                    }
                    else
                    {
                        //太陽がカメラ背面、または真横近くでwが不安定
                        sun_uv_.x = 0.5f;
                        sun_uv_.y = 0.5f;
                        sun_visible_ = 0.0f;
                    }

				}
				else
				{
					sun_uv_.x = 0.5f;
					sun_uv_.y = 0.5f;
					sun_visible_ = 0.0f;
				}
				constant.sun_param.x = sun_uv_.x;
				constant.sun_param.y = sun_uv_.y;
				constant.sun_param.z = sun_visible_;

                context->UpdateSubresource(
                    camera_buffer_.Get(), 0, nullptr, &constant, 0, 0);
                Graphics::Instance().SetConstantBuffer(
                    ConstantBufferSlot::kCamera, 1, camera_buffer_.GetAddressOf());
            }
        });
}
