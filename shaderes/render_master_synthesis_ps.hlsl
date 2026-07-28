#include"fullscreen_quad.hlsli"
#include"fog.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
#define SHADOWMAP 6
SamplerState sampler_states[7] : register(s0);

Texture2D<float4> back_texture:register(t0);
Texture2D<float4> object_texture:register(t1);
Texture2D<float> depth_texture : register(t2);
Texture2D<float4> fog_texture:register(t3);

//フォグの合成の際、フォグにあるジッターを消すためのもの
float SampleFogBlur(float2 uv)
{
    uint fog_width, fog_height;
    fog_texture.GetDimensions(fog_width, fog_height);
    
    float accumulated_weight = 0.0f;
    float accumulated_radiance = 0.0f;
    const float radius = 4.0f;
    float depth = depth_texture.Sample(sampler_states[POINT_CLAMP], uv).r;
    
    float fog_factor = 0.0f;
    for (float x = -radius; x <= radius;x+=1.0f)
    {
        for (float y = -radius; y <= radius;y+=1.0f)
        {
            float2 offset = float2(x, y) / float2(fog_width, fog_height);
            float2 texcoord = uv + offset;
            
            float4 fog_sample = fog_texture.Sample(sampler_states[LINEAR_CLAMP], texcoord);
            float sampled_radiance = fog_sample.r;
            
            float distance = x * x + y * y;
            const float sigma = 2.0f * radius * radius;
            float domain_gaussian = exp(-distance / sigma);
            
            float sample_depth = fog_sample.a;
            distance = (depth - sample_depth) * (depth - sample_depth);
            const float sigma2 = 0.0001f;
            float range_gaussian = exp(-distance / sigma2);
            
            accumulated_radiance += sampled_radiance * domain_gaussian * range_gaussian;
            accumulated_weight += domain_gaussian * range_gaussian;
            
        }
    }
    
    if (accumulated_weight <= 0.00001f)
    {
        return fog_texture.Sample(sampler_states[POINT_CLAMP], uv);
    }


    return (accumulated_radiance / accumulated_weight);
    
}


float4 main(VS_OUT pin):SV_TARGET
{
    float3 color;
    float2 uv = pin.texcoord;

    float4 back = back_texture.Sample(sampler_states[LINEAR_CLAMP], uv);
    float4 obj = object_texture.Sample(sampler_states[LINEAR_CLAMP], uv);
    
    // α合成（オブジェクトが前）
    color.rgb = obj.rgb + (back.rgb * (1.0f - obj.a));
    color.rgb += fog_color.rgb * fog_color.a*fog_intensity * SampleFogBlur(uv);
    
    
    return float4(color.rgb, 1.0f);
}