#ifndef __FOG_HLSLI__
#define __FOG_HLSLI__

cbuffer FOG_CONSTANT_BUFFER : register(b6)
{
    int fog_steps;
    float fog_max_distance;
    float noise_scale;
    float fog_density;
    
    float fog_max_height;
    float fog_intensity;
    float object_resolution_width;
    float object_resolution_height;
    
    float4 fog_color;
};
#endif// __FOG_HLSLI__