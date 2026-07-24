//カメラ情報
#ifndef CAMERA_BUFFER_
#define CAMERA_BUFFER_
cbuffer CameraBuffer : register(b7)
{
    float4 camera_position;
    float4 camera_direction;
    float4 camera_clip_distance; //x:near,y:far,z:near * far,w:far-near
    row_major float4x4 view_transform;
    row_major float4x4 projection_transform;
    row_major float4x4 view_projection_transform;
    row_major float4x4 inverse_view_transform;
    row_major float4x4 inverse_projection_transform;
    row_major float4x4 inverse_view_projection_transform;
    row_major float4x4 previous_view_projection_transform;
    float4 sun_param; //x:uv_x,y:uv_y,z:sun_visible,w:null
};

#endif // CAMERA_BUFFER_