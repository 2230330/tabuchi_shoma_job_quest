#include"fullscreen_quad.hlsli"
#include"scene_constant_buffer.hlsli"
#include"gbuffer.hlsli"
#include"filter_functions.hlsli"
#include"shading_functions.hlsli"

Texture2D<float4> scene_color_map : register(t0);
Texture2D<float4> history_color_map : register(t1);

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
SamplerState sampler_states[5] : register(s0);

#define FXAA_REDUCE_MIN (1.0f/128.f)
#define FXAA_REDUCE_MUL (1.0f/8.f)
#define FXAA_SPAN_MAX 8.f
#define FXAA_SUBPIX_SHIFT (1.0f/4.0f)

float3 fxaa(Texture2D color_texture,float2 texcoord,float2 pixel_size)
{
    //  周辺ピクセルを参照
    float2 texcoord2 = texcoord.xy - (pixel_size * (0.5 + FXAA_SUBPIX_SHIFT));
    float3 sampled_nw = color_texture.Sample(sampler_states[LINEAR_WRAP], texcoord2.xy).xyz;
    float3 sampled_ne = color_texture.Sample(sampler_states[LINEAR_WRAP], texcoord2.xy, float2(1, 0)).xyz;
    float3 sampled_sw = color_texture.Sample(sampler_states[LINEAR_WRAP], texcoord2.xy, float2(0, 1)).xyz;
    float3 sampled_se = color_texture.Sample(sampler_states[LINEAR_WRAP], texcoord2.xy, float2(1, 1)).xyz;
    float3 sampled_m = color_texture.Sample(sampler_states[LINEAR_WRAP], texcoord.xy).xyz;

    //  それぞれの輝度を算出
    float luma_nw = convert_rgb_to_luminance(sampled_nw);
    float luma_ne = convert_rgb_to_luminance(sampled_ne);
    float luma_sw = convert_rgb_to_luminance(sampled_sw);
    float luma_se = convert_rgb_to_luminance(sampled_se);
    float luma_m = convert_rgb_to_luminance(sampled_m);

    //  輝度差の最小最大を取得
    float luma_min = min(luma_m, min(min(luma_nw, luma_ne), min(luma_sw, luma_se)));
    float luma_max = max(luma_m, max(max(luma_nw, luma_ne), max(luma_sw, luma_se)));

    //  輝度差から向きベクトルを算出
    float2 dir;
    dir.x = -((luma_nw + luma_ne) - (luma_sw + luma_se));
    dir.y = +((luma_nw + luma_sw) - (luma_ne + luma_se));

    //  複数ピクセルを横断するようにエッジを伸ばす
    float dir_reduce = max((luma_nw + luma_ne + luma_sw + luma_se) * (0.25f * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    float rcp_dir_min = 1.0f / (min(abs(dir.x), abs(dir.y)) + dir_reduce);
    dir = min(float2(FXAA_SPAN_MAX, FXAA_SPAN_MAX), max(float2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcp_dir_min)) * pixel_size;

    //  向きベクトルを元に複数位置の色をブレンドして返す
    float3 rgb_a = (1.0f / 2.0f)
                 * (color_texture.Sample(sampler_states[LINEAR_WRAP], texcoord.xy + dir * (1.0f / 3.0f - 0.5f)).xyz +
                    color_texture.Sample(sampler_states[LINEAR_WRAP], texcoord.xy + dir * (2.0f / 3.0f - 0.5f)).xyz);
    float3 rgb_b = rgb_a * (1.0f / 2.0f)
                 + (1.0f / 4.0f) * (color_texture.Sample(sampler_states[LINEAR_WRAP], texcoord.xy + dir * (0.0f / 3.0f - 0.5f)).xyz +
		                            color_texture.Sample(sampler_states[LINEAR_WRAP], texcoord.xy + dir * (3.0f / 3.0f - 0.5f)).xyz);
    float luma_b = convert_rgb_to_luminance(rgb_b);
    return ((luma_b < luma_min) || (luma_b > luma_max)) ? rgb_a : rgb_b;
}

float4 main(VS_OUT pin):SV_Target
{
    float4 color = scene_color_map.Sample(sampler_states[LINEAR_WRAP], pin.texcoord);
    
    float2 pixel_size;
    scene_color_map.GetDimensions(pixel_size.x, pixel_size.y);
    
    color.rgb = fxaa(scene_color_map, pin.texcoord, rcp(pixel_size));
    
    return float4((color.rgb), color.a);
}