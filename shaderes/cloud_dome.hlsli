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

cbuffer CLOUD_RAY_MARCHING_CONSTNAT_BUFFER : register(b12)
{
    int iteration;
    float intensity;
    float fog_scale;
    float step_size;
    
    float max_distance;
    float noise_intensity;
    float noise_threshold;
    float noise_seed;
    
    float alpha_scale;
    float light_scatter_strength;
    float base_brightness;
    float padding1;
    
    float4 wind_direction;
    
    float cloud_base;
    float cloud_top;
    float2 padding2;
};