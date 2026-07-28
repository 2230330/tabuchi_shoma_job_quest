//BLOOM
#ifndef __BLOOM_HLSLI__
#define __BLOOM_HLSLI__
cbuffer BloomComputeConstants : register(b5)
{
    float bloom_extraction_threshold;
    float bloom_intensity;
    float bloom_soft_knee;
    float bloom_radius;

    uint input_width;
    uint input_height;
    uint output_width;
    uint output_height;
};

#endif