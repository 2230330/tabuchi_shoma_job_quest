#include"cloud_dome.hlsli"
#include"forward_light.hlsli"
#include "scene_constant_buffer.hlsli"
#include "noise_functions.hlsli"

static const float PI = 3.14159265359f;

// 雲ノイズ（FBM＋Warp）
float SampleCloudNoise(float3 p)
{
    // 座標をWarpして複雑化
    p = Warp(p * 0.001f);

    // FBMで雲形状
    float fbm = FBM(p);

    // コントラスト強化
    fbm = pow(saturate(fbm), 1.5f);

    return fbm;
}

// 雲密度
float SampleCloudDensity(float3 pos)
{
    if (pos.y < cloud_base || pos.y > cloud_top)
        return 0.0f;

    // 高度フォールオフ
    float h = saturate((pos.y - cloud_base) / (cloud_top - cloud_base));
    float bottom = exp(-h * h * 4.0f);
    float top_fade = 1.0f - smoothstep(0.6f, 1.0f, h);
    float height_factor = bottom * top_fade;

    // ノイズで形状
    float noise = SampleCloudNoise(pos + CurlNoise(pos * 0.01f)*noise_seed);
    float density = smoothstep(noise_threshold, noise_threshold + 0.2f, noise);

    return density * height_factor;
}

// Mie散乱フェーズ関数
float MiePhase(float cos_theta, float g)
{
    float g2 = g * g;
    return (1.0f - g2) / (pow(1.0f + g2 - 2.0f * g * cos_theta, 1.5f) * 4.0f * PI);
}

// 光透過率（簡易シャドウ）
float SampleLightTransmittance(float3 rayOrigin, float3 lightDir, float max_dist)
{
    const int LIGHT_STEPS = 4;
    float step = max_dist / LIGHT_STEPS;
    float t = 0.0f;
    float T = 1.0f;
    [unroll]
    for (int i = 0; i < LIGHT_STEPS; ++i)
    {
        float3 p = rayOrigin + lightDir * t;
        float d = SampleCloudDensity(p);
        T *= exp(-d * step);
        if (T < 0.01f)
            return 0.0f;
        t += step;
    }
    return T;
}

// メインレンダリング
float4 main(VS_OUT pin) : SV_TARGET
{
    float2 ndc = (pin.position.xy / viewport_size.xy) * 2.0f - 1.0f;
    ndc.y = -ndc.y;

    float4 end_pos = mul(float4(ndc, 1.0f, 1.0f), inverse_view_projection_transform);
    end_pos /= end_pos.w;

    float3 ray_start = camera_position.xyz;
    float3 ray_dir = normalize(end_pos.xyz - ray_start);

    float3 sun_dir = normalize(-directional_light.direction.xyz);
    float cosTheta = dot(ray_dir, sun_dir);
    float phase = MiePhase(cosTheta, 0.85f); // 強い前方散乱

    if (dot(ray_dir, float3(0, 1, 0)) < 0.01f)
        return float4(0, 0, 0, 0);

    float step_size = (cloud_top - cloud_base) / iteration;
    float distance = 0.0f;

    float transmittance = 1.0f;
    float3 color = 0;

    [loop]
    for (int i = 0; i < iteration && distance < max_distance; i++)
    {
        float3 ray_position = ray_start + ray_dir * distance;

        if (ray_position.y > cloud_base && ray_position.y < cloud_top)
        {
            float density = SampleCloudDensity(ray_position);

            if (density > 0.001f)
            {
                float tau = density * step_size * intensity;
                float step_transmittance = exp(-tau);

                float light_t = SampleLightTransmittance(ray_position, sun_dir, cloud_top - cloud_base);
                float scatter = density * light_t * light_scatter_strength * intensity * phase;

                color += transmittance * scatter * step_size;
                transmittance *= step_transmittance;

                if (transmittance < 0.01f)
                    break;
            }
        }

        distance += step_size;
    }

    // トーンマッピング＋空色ブレンド
    float3 final_color = color / (color + 1.0f); // Reinhardトーンマップ

    float alpha = saturate(1.0f - transmittance) * alpha_scale;
    return float4(final_color, alpha);
}