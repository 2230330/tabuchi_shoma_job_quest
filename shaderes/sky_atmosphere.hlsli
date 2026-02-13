struct VS_IN
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 world_pos : TEXCOORD1;
};


cbuffer RAYLEIGH_CONSTANT_BUFFER_ : register(b11)
{
    float rayleigh_scale_height;
    float mie_scale_height;
    float ozone_scale_half_width;
    float ozone_center_height;
    float earth_height; // 地球半径 [m]
    float sun_distance; // 太陽までの距離 [m]
    float atmosphere_height; // 大気の高さ [m]
    int max_sample;

    float height;//自身の高度
    float3 dummy;

};