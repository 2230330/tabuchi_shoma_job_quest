//ゴッドレイに使う、簡易的な雲の存在マップ
#include"volumetric_cloud.hlsli"
#include"fullscreen_quad.hlsli"
#include "scene_constant_buffer.hlsli"
#include"forward_light.hlsli"

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

static const float PI = 3.14159265359f;

static const float time_offset = 10000.0;
float4 SampleLowFrequencyNoises(float3 sample_point, float mip_level)
{
    return low_frequency_perlin_worley_texture.SampleLevel(sampler_states[LINEAR_MIRROR], sample_point * low_frequency_perlin_worley_sampling_scale, mip_level);
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

// 指定された位置における雲の密度を返す
float SampleCloudDensity
(float3 sample_point,
float3 weather_data,
float mip_level,
bool cheap_sample = false,
float horizon_soft = 0)
{
	// 高さに応じてノイズをブレンドするため、位置から高さ比率を算出
    float height_fraction = GetHeightFractionForPoint(length(sample_point));
	
	// 雲の被覆率(カバレッジ)はweather_dataのRチャンネルに格納されている
    float cloud_coverage = weather_data.r * cloud_coverage_scale;
    
#ifdef ENABLE_CLOUD_ANIMATION
        // 風向きと風速に基づいて雲を移動させる（低周波形状用）
    sample_point.xz += (options.z + time_offset) * 20.0 * normalize(-wind_direction) * wind_speed * 0.6;
#endif
	// 低周波のPerlin-WorleyノイズとWorleyノイズを取得
    float4 low_frequency_noises = SampleLowFrequencyNoises(sample_point, max(mip_level - 2.0, 0.0f));

	// 低周波 Worley ノイズを用いて fbm（フラクタルブラウン運動）を構築し、
    // 低周波 Perlin-Worley ノイズにディテールを追加する
    float low_frequency_fbm =
    low_frequency_noises.g * 0.625 +
    low_frequency_noises.b * 0.25 +
    low_frequency_noises.a * 0.125;
    
	// 低周波 fbm によって Perlin-Worley ノイズを膨張させ、
	// 雲のベース形状を定義する
    float base_cloud =
    Remap(low_frequency_noises.r,
    -(1.0 - low_frequency_fbm),
    1.0,
    0.0,
    1.0);
	
	// 雲タイプに応じた、高さ方向の密度勾配を取得
    float cloud_type = clamp(weather_data.b * cloud_type_scale, 0.0, 1.0);
    float density_height_gradient = GetDensityHeightGradient(height_fraction, cloud_type);
	
	// 高さ方向の密度関数をベース雲形状に適用
    base_cloud *= density_height_gradient;
	
	
	// remapを用いて雲の被覆率を反映させる
    float base_cloud_with_coverage = Remap(base_cloud, 1.0 - cloud_coverage, 1.0, 0.0, 1.0);
    base_cloud_with_coverage *= cloud_coverage;

    float final_cloud = base_cloud_with_coverage;
    if (!cheap_sample && base_cloud_with_coverage > 0.0)
    {
		// 雲の上部をWorleyノイズで削り、もくもくした立体感を作る
	
#ifdef ENABLE_CLOUD_ANIMATION
        // 高周波ディテール用の追加アニメーション
        if (wind_speed != 0.0)
        {
            sample_point.xz -= (options.z + time_offset) * normalize(-wind_direction) * 40.0;
            sample_point.y -= (options.z + time_offset) * 40.0;
        }
#endif
		
		
		// 高周波ノイズをサンプリング
        float3 high_frequency_noises = SampleHighFrequencyNoises(sample_point, mip_level);
	
		// 高周波 Worley ノイズによる fbm を構築
        float high_frequency_fbm =
        high_frequency_noises.r * 0.625 +
        high_frequency_noises.g * 0.25 +
        high_frequency_noises.b * 0.125;
	
		// 高さに応じて、繊細な雲形状から膨らんだ形状へ遷移させる
#if 1
        float high_frequency_noise_modifier =
        lerp(high_frequency_fbm, 1.0 - high_frequency_fbm, clamp(height_fraction * 10.0, 0.0, 1.0));
#else
        float high_frequency_noise_modifier = lerp(high_frequency_fbm, 1.0 - high_frequency_fbm, clamp(height_fraction * 4.0, 0.0, 1.0));
#endif
	
		// 歪ませた高周波Worleyノイズでベース雲形状を侵食する
#if 1
        final_cloud = Remap(base_cloud_with_coverage, high_frequency_noise_modifier * 0.2, 1.0, 0.0, 1.0);
#else	
		final_cloud = remap(base_cloud_with_coverage, high_frequency_noise_modifier * 0.4 * height_fraction, 1.0, 0.0, 1.0);
#endif
    }

#if 1
    return clamp(final_cloud, 0.0, 1.0);
#else
    return pow(clamp(final_cloud, 0.0, 1.0), (1.0 - height_fraction) * 0.8 + 0.5);
#endif
}


//深度情報ではなく、透過率で雲の存在を返す軽量化版RayMarch
float RayMarch(
float3 ray_origin,
float3 ray_step,
int steps,
float3 coarse_weather)
{
    // レイの1ステップの長さ
    float step_size = length(ray_step);
    
    //レイの方向
    const float3 ray_direction = normalize(ray_step);
	
	// レイ開始位置を少しランダムにして横筋を目立たなくする（ディザリング）
    float3 sample_point = ray_origin + ray_step * Hash(ray_origin * 6.0);

    float view_up = saturate(dot(normalize(ray_step), float3(0, 1, 0)));
    float horizon = 1.0 - view_up;
    float horizon_soft = lerp(0.2, 0.8, horizon);

    float cloud_test = 0.0;
    int zero_density_sample_count = 0;
    float detail_lod = smoothstep(0.0, 10.0, horizon_soft);

    float transmittence = 1.0;
    
    //距離制限
    float dist = 0;
    const float limit_dist = length(ray_step) * steps;
    
    [loop]
    for (int i = 0; i < steps; ++i)
    {
        if (cloud_test > 0.0)
        {
            float d = SampleCloudDensity(sample_point, coarse_weather, detail_lod, false, horizon);

            if (d == 0)
            {
                zero_density_sample_count++;
            }
            if (zero_density_sample_count != 6)
            {
                float sigma = density_scale * d;
                //float T_step = 1.0f / (1.0f+sigma*step_size);
                //float T_step = exp(-sigma * step_size);
                float T_step = exp2((-sigma * step_size)*1.44269504);

                transmittence *= T_step;

                if (transmittence < 1e-2f)
                    break;
            }
            else
            {
                cloud_test = 0.0;
                zero_density_sample_count = 0;
            }

            sample_point += ray_step;
        }
        else
        {
            cloud_test = SampleCloudDensity(sample_point, coarse_weather, 6.0, true);
            float step_scale = 1.0f;
            if (cloud_test <= 0.0)
            {
                //地平線は荒く、上空は細かくステップを進める
                //step_scale = lerp(2.0f, 6.0f, horizon);
            }
            sample_point += ray_step * step_scale;
        }
        

    }

    float alpha = saturate(1.0 - transmittence);
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

    if (ray_dir.y < 0.0)
        clip(-1);

    float sun_distance = 1.0f;
    float3 eye_pos = float3(0.0, earth_radius, 0.0) + camera_position.xyz;
    float t0 = IntersectSphere(eye_pos, ray_dir, cloud_altitudes_min_max.x);
    float t1 = IntersectSphere(eye_pos, ray_dir, cloud_altitudes_min_max.y);

    // 雲レイヤーに交差しない場合はスキップ
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
    int steps = max(1, (int) (base_steps * lerp(0.1, 1.0f, saturate(1.0 - coarse_coverage))));
    float3 ray_step = ray_dir * (shell_dist / steps);

    float cloud_presence = RayMarch(ray_origin, ray_step, steps, coarse_weather);
    
    cloud_presence = saturate(cloud_presence);
    
    //深度情報
    float depth_norm = saturate(t0 / t1);
    //雲がないピクセルは最遠に設定
    if(cloud_presence<=0.001f)
        depth_norm = 1.0f;
     
    // 雲の存在を赤チャンネルに格納、深度情報をgチャンネルに格納
    return float4(cloud_presence, depth_norm, 0, 1);
}