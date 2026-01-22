//ライトシャフトに使う、簡易的な雲の存在マップ（軽量化版）
// 改良点: 雲被覆率に基づく早期スキップ、天候テクスチャのループ外キャッシュ、cheap_sample 分岐の早期簡略化、レイマーチングステップ数の動的縮小
#include"cloud_dome.hlsli"
#include"fullscreen_quad.hlsli"
#include "scene_constant_buffer.hlsli"
#include"forward_light.hlsli"
#include"gbuffer.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
SamplerState sampler_states[6] : register(s0);

Texture3D<float4> low_frequency_perlin_worley_texture : register(t0);
Texture3D<float3> high_frequency_worley_texture : register(t1);
Texture2D<float3> weather_texture : register(t2);
Texture2D<float3> curl_noise_texture : register(t3);
Texture2D<float3> sky_color_texture : register(t4);

static const float time_offset = 10000.0;
float4 SampleLowFrequencyNoises(float3 sample_point, float mip_level)
{
    return low_frequency_perlin_worley_texture.SampleLevel(sampler_states[LINEAR_WRAP], sample_point * low_frequency_perlin_worley_sampling_scale, mip_level);
}
float3 SampleHighFrequencyNoises(float3 sample_point, float mip_level)
{
    return high_frequency_worley_texture.SampleLevel(sampler_states[LINEAR_WRAP], sample_point * high_frequency_worley_sampling_scale, mip_level);
}
float3 SampleWeatherData(float2 sample_point)
{
    float2 offset = ((options.z + time_offset) * 0.001) * normalize(-wind_direction) * wind_speed;

    float horizon_distance = sqrt(cloud_altitudes_min_max.x * cloud_altitudes_min_max.x - earth_radius * earth_radius) * horizon_distance_scale;
    return weather_texture.Sample(sampler_states[LINEAR_WRAP], float2(sample_point.x + horizon_distance, horizon_distance - sample_point.y) / (2.0 * horizon_distance) + offset);

}

static const float PI = 3.14159265359f;

// from: https://www.shadertoy.com/view/4sfgzs credit to iq
float Hash(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float Remap(float original_value, float original_min, float original_max, float new_min, float new_max)
{
    float denom = original_max - original_min;
    if (abs(denom) < 1e-6f)
        return new_min;
    return new_min + (((original_value - original_min) / denom) * (new_max - new_min));
}

// 雲レイヤー内におけるサンプル位置の高さ比率を求める
float GetHeightFractionForPoint(float position)
{
    float height_fraction = (position - cloud_altitudes_min_max.x) / (cloud_altitudes_min_max.y - cloud_altitudes_min_max.x);
    return clamp(height_fraction, 0.0, 1.0);
}

// 雲タイプに応じた高さ方向の密度勾配を取得
float GetDensityHeightGradient(float height_fraction, float cloud_type)
{
    float density_gradient = 0.0;

    const float4 stratus_gradient = float4(0.02f, 0.05f, 0.09f, 0.11f);
    const float4 stratocumulus_gradient = float4(0.02f, 0.2f, 0.48f, 0.625f);
    const float4 cumulus_gradient = float4(0.01f, 0.0625f, 0.78f, 1.0f);

    float stratus = 1.0f - clamp(cloud_type * 2.0f, 0.0, 1.0);
    float stratocumulus = 1.0f - abs(cloud_type - 0.5f) * 2.0f;
    float cumulus = clamp(cloud_type - 0.5f, 0.0, 1.0) * 2.0f;

    float4 cloud_gradient = stratus_gradient * stratus + stratocumulus_gradient * stratocumulus + cumulus_gradient * cumulus;
    density_gradient = smoothstep(cloud_gradient.x, cloud_gradient.y, height_fraction) - smoothstep(cloud_gradient.z, cloud_gradient.w, height_fraction);
    return density_gradient;
}

#define ENABLE_CLOUD_ANIMATION 

// 指定された位置における雲の密度を返す（cheap_sample で大幅に軽量化）
float SampleCloudDensity
(float3 sample_point, float3 weather_data, float mip_level, bool cheap_sample = false, float horizon_soft = 0)
{
    // 高さに応じてノイズをブレンドするため、位置から高さ比率を算出
    float height_fraction = GetHeightFractionForPoint(length(sample_point));

    // 雲の被覆率(カバレッジ)
    float cloud_coverage = weather_data.r * cloud_coverage_scale;

    // 被覆率が非常に小さい場合は即座に0を返して重い処理を回避する
    if (cloud_coverage < 0.01f)
        return 0.0f;

#ifdef ENABLE_CLOUD_ANIMATION
    sample_point.xz += (options.z + time_offset) * 20.0 * normalize(-wind_direction) * wind_speed * 0.6;
#endif

    // cheap_sample なら低周波サンプルのみで大まかな判定を行い、高周波やカールノイズは省略する
    float4 low_frequency_noises = SampleLowFrequencyNoises(sample_point, max(mip_level + 2.0, 2.0)); // 高いmipで cheaper
    float low_frequency_fbm =
        low_frequency_noises.g * 0.625 +
        low_frequency_noises.b * 0.25 +
        low_frequency_noises.a * 0.125;

    float base_cloud =
        Remap(low_frequency_noises.r,
        -(1.0 - low_frequency_fbm),
        1.0,
        0.0,
        1.0);

    float cloud_type = clamp(weather_data.b * cloud_type_scale, 0.0, 1.0);
    float density_height_gradient = GetDensityHeightGradient(height_fraction, cloud_type);
    base_cloud *= density_height_gradient;

    float base_cloud_with_coverage = Remap(base_cloud, 1.0 - cloud_coverage, 1.0, 0.0, 1.0);
    base_cloud_with_coverage *= cloud_coverage;

    if (cheap_sample)
    {
        // cheap_sample では高周波をまったく入れずに返す（ライトシャフト用途で十分）
        return clamp(base_cloud_with_coverage, 0.0, 1.0);
    }

    // 高品質パス: 高周波ノイズやカールノイズを適用
#ifdef ENABLE_CLOUD_ANIMATION
    if (wind_speed != 0.0)
    {
        sample_point.xz -= (options.z + time_offset) * normalize(-wind_direction) * 40.0;
        sample_point.y -= (options.z + time_offset) * 40.0;
    }
#endif

    // カールノイズは小さいスケールで1テクスチャフェッチに留める（頻繁にやらない）
    float3 curl_noise = curl_noise_texture.SampleLevel(sampler_states[LINEAR_MIRROR], sample_point.xy * 0.000008, 0);
    sample_point.xy = sample_point.xy + curl_noise.xy * (1.0 - height_fraction);

    float3 high_frequency_noises = SampleHighFrequencyNoises(sample_point, mip_level);
    float high_frequency_fbm =
        high_frequency_noises.r * 0.625 +
        high_frequency_noises.g * 0.25 +
        high_frequency_noises.b * 0.125;

    float high_frequency_noise_modifier =
        lerp(high_frequency_fbm, 1.0 - high_frequency_fbm, clamp(height_fraction * 10.0, 0.0, 1.0));

    float final_cloud = Remap(base_cloud_with_coverage, high_frequency_noise_modifier * 0.2, 1.0, 0.0, 1.0);
    return clamp(final_cloud, 0.0, 1.0);
}

// RayMarch を軽量化: 天候データをループ外で渡す（テクスチャフェッチ削減）
// coarse_weather: ループ外でサンプリングした天候（各ステップで同じものを使う）
float RayMarch(float3 ray_origin, float3 ray_step, int steps, float2 texcoord /*背景の空の色*/, float3 coarse_weather)
{
    float step_size = length(ray_step);
    const float3 ray_direction = normalize(ray_step);

    // 乱雑化は1回だけ
    float3 sample_point = ray_origin + 
    ray_step * Hash(ray_origin * 6.0);


    float3 sun = -directional_light.direction.xyz;
    float3 sun_direction = normalize(sun);

    float view_up = saturate(dot(normalize(ray_step), float3(0, 1, 0)));
    float horizon = 1.0 - view_up;
    float horizon_soft = lerp(0.2, 0.8, horizon);

    float density = 0.0;
    float cloud_test = 0.0;
    int zero_density_sample_count = 0;

    [loop]
    for (int i = 0; i < steps; i++)
    {
        if (cloud_test > 0.0)
        {
            // ループ内テクスチャフェッチを削減: coarse_weather を使う
            float detail_lod = smoothstep(0.0, 10.0, horizon_soft);
            float sampled_density =
                SampleCloudDensity(
                    sample_point,
                    coarse_weather,
                    detail_lod,
                    (detail_lod <= 1.0f) ? false : true /*cheap_sample*/,
                    horizon);

            if (sampled_density == 0.0)
            {
                zero_density_sample_count++;
            }

            if (zero_density_sample_count != 3)
            {
                float density_attenuation = lerp(1.0f, 0.3f, horizon_soft);
                density += sampled_density * density_attenuation;

                if (density > 0.5f)
                    break;

                sample_point += ray_step;
            }
            else
            {
                cloud_test = 0.0;
                zero_density_sample_count = 0;
            }
        }
        else
        {
            // 低品質サンプリング: coarse_weather を使って判定するためテクスチャフェッチ不要
            cloud_test = SampleCloudDensity(sample_point, coarse_weather, 0.0, true /*cheap_sample*/);
            float step_scale = 1.0f;
            if (cloud_test <= 0.0)
            {
                step_scale = lerp(1.0f, 3.0f, horizon);
            }
            sample_point += ray_step * step_scale;
        }
    }

    float optical_thickness = density_scale * density * step_size;
    float alpha = saturate(1.0 - exp(-optical_thickness));

    return alpha;
}

// レイと球の交差判定
float IntersectSphere(float3 pos, float3 dir, float r)
{
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, pos);
    float c = dot(pos, pos) - (r * r);
    float disc = b * b - 4.0 * a * c;
    if (disc < 0.0f)
    {
        return -1.0f;
    }
    float d = sqrt(disc);
    float t0 = (-b - d) / (2.0 * a);
    float t1 = (-b + d) / (2.0 * a);
    if (t0 > 0.0f)
        return t0;
    return t1;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 ndc = float4(2.0 * pin.texcoord.x - 1.0, 1.0 - 2.0 * pin.texcoord.y, 0.0, 1.0);
    float4 pos = mul(ndc, inverse_view_projection_transform);
    pos /= pos.w;

    float3 ray_dir = normalize(pos.xyz - camera_position.xyz);
    float3 light_dir = normalize(-directional_light.direction.xyz);

    if (ray_dir.y < 0.0)
        return float4(0, 0, 0, 0);

    float3 eye_pos = float3(0.0, earth_radius, 0.0) + camera_position.xyz;
    float t0 = IntersectSphere(eye_pos, ray_dir, cloud_altitudes_min_max.x);
    float t1 = IntersectSphere(eye_pos, ray_dir, cloud_altitudes_min_max.y);

    if (t0 < 0.0 || t1 < 0.0)
        return float4(0, 0, 0, 0);

    float3 ray_origin = eye_pos + ray_dir * t0;
    float3 ray_endpoint = eye_pos + ray_dir * t1;
    float shell_dist = length(ray_endpoint - ray_origin);

    int base_steps = ray_marching_steps;

    // ループ内サンプリングを避けるため、中間点で天候を1回だけサンプリング
    float3 mid_point = (ray_origin + ray_endpoint) * 0.5;
    float3 coarse_weather = SampleWeatherData(mid_point.xz);
    float coarse_coverage = coarse_weather.r * cloud_coverage_scale;

    // 小さい被覆率なら完全にスキップ（ライトシャフト目的なら小さい雲は無視）
    const float SKIP_COVERAGE_THRESHOLD = 0.03f;
    if (coarse_coverage < SKIP_COVERAGE_THRESHOLD)
        return float4(0, 0, 0, 1);

    // 被覆率に応じてステップ数を縮小（低コスト）
    int steps = max(1, (int) (base_steps * lerp(0.1f, 1.0f, saturate(coarse_coverage))));
    float3 ray_step = ray_dir * (shell_dist / steps);

    float cloud_presence = RayMarch(ray_origin, ray_step, steps, pin.texcoord.xy, coarse_weather);
    return float4(cloud_presence, 0, 0, 1);
}