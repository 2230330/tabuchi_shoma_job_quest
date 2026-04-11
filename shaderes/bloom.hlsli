//BLOOM
cbuffer BLOOM_CONSTANT_BUFFER : register(b5)
{
    float bloom_extraction_threshold;
    float bloom_intensity;
    float bloom_soft_knee;
    float bloom_radius;
}