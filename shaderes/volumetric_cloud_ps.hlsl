#include"volumetric_cloud.hlsli"
#include"forward_light.hlsli"
#include "scene_constant_buffer.hlsli"
//#include "noise_functions.hlsli"
#include "fullscreen_quad.hlsli"

static const float PI = 3.14159265359f;

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
SamplerState sampler_states[6] : register(s0);

/*
	low_frequency_perlin_worley_texture
	r channel is perln-worley noise, gba channels are worley noise at increasing frequencies 
*/
Texture3D<float4> low_frequency_perlin_worley_texture : register(t0);
/*
	low_frequency_perlin_worley_texture
	rgb channels are worley noise at increasing frequencies 
*/

Texture3D<float3> high_frequency_worley_texture : register(t1);
/*
	weather_texture
	- cloud coverage(r channel): the precentage of cloud coverage in the sky
	- precipitation(g channel): the chance that the cloud overhead will produce rain
	- cloud type(b channel): a value of 0.0 indicates stratus, 0.5 indicates stratocumulus, and 1.0 indicate cumulus cloud
*/
Texture2D<float3> weather_texture : register(t2);
Texture2D<float3> curl_noise_texture : register(t3);
Texture2D<float3> sky_color_texture : register(t4);

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

// utility function that maps a value from one range to another
float Remap(float original_value, float original_min, float original_max, float new_min, float new_max)
{
    float denom = original_max - original_min;
    if (abs(denom) < 1e-6f)
        return new_min;
    return new_min + (((original_value - original_min) / denom) * (new_max - new_min));
}


static const float EARTH_RADIUS = 6370000.0f; // 地球半径 [m]
static const float SUN_DISTANCE = 150000000000.0f; // 太陽までの距離 [m]

//フェーズ関数(ミー散乱)
float MiePhase(float cos_theta, float g)
{
    //ヘニエイ・グリーンスタイン関数
    float g2 = g * g;
    
    return (1.0f - g2) / (pow(1.0f + g2 - 2.0f * g * cos_theta, 1.50f) * 4.0f * PI);
}



// 雲レイヤー内におけるサンプル位置の高さ比率を求める
float GetHeightFractionForPoint(float position)
{
	// 雲が存在する高度範囲内でのグローバルな相対位置（0.0~1.0）を計算
    float height_fraction = (position - cloud_altitudes_min_max.x) / (cloud_altitudes_min_max.y - cloud_altitudes_min_max.x);
    return clamp(height_fraction, 0.0, 1.0);
}

// 雲タイプに応じた高さ方向の密度勾配を取得
float GetDensityHeightGradient(float height_fraction, float cloud_type)
{
    float density_gradient = 0.0;

    // height_fraction に基づいて、stratus、stratocumulus、cumulus の各雲タイプの密度勾配をブレンドする
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
// レイと球の交差判定を行い、交差点までの距離を返す
float IntersectSphere(float3 pos, float3 dir, float r)
{
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, pos);
    float c = dot(pos, pos) - (r * r);
    float disc = b * b - 4.0 * a * c;
    if (disc < 0.0f)
    {
        return -1.0f; // 交差なしを示す（呼び出し側で扱う）
    }
    float d = sqrt(disc);
    float t0 = (-b - d) / (2.0 * a);
    float t1 = (-b + d) / (2.0 * a);
    // 小さい正の t を返す（典型的）
    if (t0 > 0.0f)
        return t0;
    return t1;
}

#define ENABLE_CLOUD_ANIMATION 
// 指定された位置における雲の密度を返す
float SampleCloudDensity
(float3 sample_point,
float3 weather_data, 
float mip_level,
bool cheap_sample = false,
float horizon_soft=0)
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
		
#if 1
		// 雲の下部に乱流（カールノイズ）を加えて自然な揺らぎを作る
        float3 curl_noise = curl_noise_texture.SampleLevel(sampler_states[LINEAR_MIRROR], sample_point.xy * 0.000008, 0);
        sample_point.xy = sample_point.xy + curl_noise.xy * (1.0 - height_fraction);

#endif
		
		// 高周波ノイズをサンプリング
        float3 high_frequency_noises =SampleHighFrequencyNoises(sample_point, mip_level);
	
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

// 円錐内でのサンプリング方向をばらけさせるためのオフセットベクトル群
static const float3 noise_kernel[6] =
{
    float3(0.38051305f, 0.92453449f, -0.02111345f),
    float3(-0.50625799f, -0.03590792f, -0.86163418f),
    float3(-0.32509218f, -0.94557439f, 0.01428793f),
    float3(0.09026238f, -0.27376545f, 0.95755165f),
    float3(0.28128598f, 0.42443639f, -0.86065785f),
    float3(-0.16852403f, 0.14748697f, 0.97460106f)
};


// 円錐内での光学的厚みサンプリング
//雲内部で収束するように改良
// 円錐内での光学的厚みサンプリング
float SampleCloudOpticalDepthAlongCone(
    float3 ray_origin,
    float3 ray_direction, // normalized
    float step_length
)
{
    float3 sample_point = ray_origin;
    float optical_depth = 0.0f;

    const int loop_samples = 6;
    const int cone_samples = 6;

    
    [unroll]
    for (int i = 0; i < loop_samples; i++)
    {
        float distance_t = i / (float) (loop_samples - 1);
        float base_cone_width = step_length * lerp(0.4, 1.2, distance_t);

        float3 weather_data = SampleWeatherData(sample_point.xz);

        float mip = (float) i;
        float density = SampleCloudDensity(
            sample_point,
            weather_data,
            mip,
            false
        );

        // 雲内部で cone を収束（擬似多重散乱 proxy）
        float cone_attenuation = 1.0 - saturate(density * 2.0);
        float cone_width = base_cone_width * cone_attenuation;

        // 密度に応じたローカルステップ
        float local_step =
            step_length * lerp(1.0, 0.4, saturate(density * 3.0));

        // サンプル位置更新
        float3 up = abs(ray_direction.y) < 0.99 ? float3(0, 1, 0) : float3(1, 0, 0);
        float3 tangent = normalize(cross(up, ray_direction));
        float3 bitangent = cross(ray_direction, tangent);

        float2 cone_noise = noise_kernel[i % cone_samples]; // [-1,1]

        sample_point +=
        ray_direction * local_step +
        (tangent * cone_noise.x + bitangent * cone_noise.y) * cone_width;


        // -----------------------------
        // 非線形 optical depth 積算
        // -----------------------------
        float sigma = density * local_step;
        optical_depth += 1.0f - exp(-sigma); // 収束が十分なら打ち切り
        if (optical_depth > 5.0)
            break;

    }

    return optical_depth;
}

// long distance light sample combined with the cone samples
float SampleCloudDensityLongDistance(float3 ray_origin, float3 ray_direction /*normalized*/, float cone_spread_multplier /*how wide to make the cone*/)
{
	//const float cloud_density_long_distance_scale = 18.0;
    const float long_distance = cloud_density_long_distance_scale * cone_spread_multplier;
	
    float3 sample_point = ray_origin + ray_direction * long_distance;
	
    float height_fraction = GetHeightFractionForPoint(length(sample_point));
    float3 weather_data = SampleWeatherData(sample_point.xz);
	
    return pow(SampleCloudDensity(sample_point, weather_data, 5.0 /*mip_level*/, false /*cheap_sample*/), (1.0 - height_fraction) * 0.8 + 0.5);
}

//太陽光の色を取得
static const float RAYLEIGH_SCALE_HEIGHT = 8000.f;
static const float MIE_SCALE_HEIGHT = 1200.f;
static const float OZONE_SCALE_HALF_WIDTH = 15000.f;
static const float OZONE_CENTER_HEIGHT = 50000.f;
//簡易的レイリー散乱
float3 SigmaRayleigh(float h)
{
    
    return float3(5.802e-6, 13.558e-6, 33.1e-6) * exp(-(h) / RAYLEIGH_SCALE_HEIGHT);
}
//簡易的ミー散乱
float3 SigmaMie(float h)
{
    return float3(3.996e-6, 3.31e-6, 1e-6f) * exp(-(h) / MIE_SCALE_HEIGHT);
}
//オゾン吸収係数
float3 SigmaOzone(float h)
{
    float3 coeff = float3(0.650e-6, 1.881e-6, 0.085e-6);
    float ozoneFactor = max(0.0, 1.0 - (abs(h - OZONE_CENTER_HEIGHT) / OZONE_SCALE_HALF_WIDTH));
    return coeff * ozoneFactor;
}
//距離×平均係数で指数減衰を近似
float3 TransmittanceApprox(float3 startPos, float3 endPos)
{
    float distance = max(50e3f, length(endPos - startPos));
    // 高度に依存した sigma_t_avg を評価するのが理想だが、まずは sample midpoint を使う
    float3 midPos = (startPos + endPos) * 0.5f;
    float h = max(0.0f, length(midPos) - EARTH_RADIUS);
    float3 sigma_t = SigmaRayleigh(h) + SigmaMie(h) + SigmaOzone(h);
    return exp(-sigma_t * distance); // component-wise exp
}
// 波長 (nm)
static const float3 WAVELENGTHS = float3(680.0f, 550.0f, 440.0f); // 赤, 緑, 青
// λ^-4 の相対比を計算
static const float3 INV_WAVELENGTH4 = (float3(
    1.0f / pow(WAVELENGTHS.x, 4.0f),
    1.0f / pow(WAVELENGTHS.y, 4.0f),
    1.0f / pow(WAVELENGTHS.z, 4.0f)
));
float3 ComputeSunIrradiance(float air_mass)
{
    // 光学的厚さ τ(λ)
    float3 tau = INV_WAVELENGTH4 * air_mass * 3e9f /*スキャッタリングスケール*/;

    // Beer-Lambert減衰
    float3 Ei = exp(-tau);

    return Ei; // 赤が残り、青が強く減衰
}


// レイマーチングによる大気散乱と雲のレンダリング
float4 RayMarch(float3 ray_origin, float3 ray_step, int steps, float2 texcoord/*背景の空の色*/)
{
    // レイの1ステップの長さ
    float step_size = length(ray_step);
    
    //レイの方向
    const float3 ray_direction = normalize(ray_step);
	
	// レイ開始位置を少しランダムにして横筋を目立たなくする（ディザリング）
    float3 sample_point =ray_origin + ray_step * Hash(ray_origin * 6.0);
	
    //太陽方向と位相関数
    float3 sun = -directional_light.direction.xyz;
    float3 sun_direction = normalize(sun);
    float cos_theta = clamp(dot(ray_direction, sun_direction), -0.f, 1.0f); //視線と太陽の角度
    float g = 0.6f;
    //float henyey_greenstein_phase = max(max(MiePhase(cos_theta, 0.4f), MiePhase(cos_theta, (0.4 - 1.4 * sun_direction.y))), MiePhase(cos_theta, -0.2)); // TODO
    float henyey_greenstein_phase = MiePhase(cos_theta, g);
    henyey_greenstein_phase = saturate(henyey_greenstein_phase); //0~1.0の範囲に圧縮

    //太陽光スペクトルの減衰(波長依存)
    float sun_elevation = clamp(dot(sun_direction, float3(0, 1, 0)), 0.0f, 1.0f); // 太陽の高さ
    float sun_theta = acos(sun_elevation) * (180.0f / PI); //度に変換
    //kasten-Young 1989近似
    float air_mass = 1.0f / (sun_elevation + (0.50572f * pow(96.07995 - sun_theta, -1.6364))); // secant近似
    air_mass = min(air_mass, 4.0f); // 極端な値を制限
     // 太陽直接光
    float3 Ei = ComputeSunIrradiance(air_mass);
    
    Ei *= lerp(0.5f, 1.5f, sun_elevation); //太陽高度で増減
    
    // 地球中心基準のカメラ位置
    float3 position = float3(0.f, earth_radius, 0.f);
    
    //大気散乱の色を取得
    float3 atmosphere_color = sky_color_texture.Sample(sampler_states[LINEAR_CLAMP],texcoord.xy);
	
    //雲の光学的厚み計算用の係数
    const float cone_spread_multplier = ((cloud_altitudes_min_max.y - cloud_altitudes_min_max.x) / 36.0); 
	
    //水平方向の光の制御
    float view_up = saturate(dot(normalize(ray_step), float3(0, 1, 0)));
    float horizon = 1.0 - view_up; // 0:上向き, 1:地平線
    float horizon_soft = lerp(0.2, 0.8, horizon);
    
    float3 color = 0.0;
    float density = 0;
    float transmittence = 1.0;
    float cloud_test = 0;
    int zero_density_sample_count = 0;
    
    float dist = 0.0f;
    const float limit_dist = length(ray_step) * steps;
	
    //メインのレイマーチングループ
	[loop]
    for (int i = 0; i < steps; i++)
    {
        //雲内部なら高品質
        if (cloud_test > 0.0)
        {
            //天候データ
            float3 weather_data = SampleWeatherData(sample_point.xz);
            //LOD調整
            float detail_lod = smoothstep(0.0, 10.0, horizon_soft);
            //雲密度サンプリング
            float sampled_density = 
            SampleCloudDensity(
            sample_point,
            weather_data,
            detail_lod,
            (detail_lod<=1.0f)?false:true /*cheap_sample*/,
            horizon);
            // 密度ゼロが続いた回数をカウント
            if (sampled_density == 0.0)
            {
                zero_density_sample_count++;
            }
            //雲内部と判断できる間は積算を続行
            if (zero_density_sample_count != 6)
            {
                float density_attenuation = lerp(1.0f, 0.3f, horizon_soft);
                // 雲密度の積算、地平線方向で
                density += sampled_density*density_attenuation;
                // 雲密度がゼロでない場合、カウントをリセット
                if (sampled_density != 0.0)
                {
                    //太陽方向の密度計算
                    float density_along_light_ray = 0.0;
                    
                    float optical_depth_along_light_ray = 0.0;
                    //太陽がカメラ側にある場合、影を作らない

                    //雲の影計算
                    {
                        optical_depth_along_light_ray += SampleCloudOpticalDepthAlongCone(
                        sample_point, sun_direction, step_size);
                        density_along_light_ray +=
                        SampleCloudDensityLongDistance(sample_point, sun_direction, cone_spread_multplier);
                        

                    }
                    //else
                    //{
                    //    optical_depth_along_light_ray += sampled_density * 10.f;
                    //}


                    //光学的厚み
                    float shadow_strength = 10.0f; //ループを6回で切る為、影が薄くなるので強めにする
                    float light_density_scale = density_scale * shadow_strength;
                    float optical_thickness = (light_density_scale * (optical_depth_along_light_ray+density_along_light_ray));
                    
                    float rain_cloud_absorption = exp(-weather_data.g) * rain_cloud_absorption_scale;

                    //地平線方向では吸収を弱める
                    float horizon_optical_scale = lerp(1.0f, 0.4f, horizon_soft);
                    float beers_law = exp(-optical_thickness * horizon_optical_scale * rain_cloud_absorption);

					// 雲内部を明るくする近似
                    float powdered_sugar = 
                    enable_powdered_sugar_efffect ? 1.0 - exp(-optical_thickness)*2.0f : 0.5;
                    powdered_sugar *= lerp(0.8f, 1.2f, horizon_soft);
					
                    //高さ
                    float height = saturate(
                    (sample_point.y - cloud_altitudes_min_max.x) /
                    (cloud_altitudes_min_max.y - cloud_altitudes_min_max.x));
                    //直射光ブースト
                    float top_light = smoothstep(0.0f, 1.0f, height);
                    float up_light = saturate(dot(sun_direction, float3(0, 1, 0)));
                    
                   float3 direct_light =
                    (1.0f
                    * beers_law
                    * powdered_sugar
                    * lerp(0.1f, 30.0f, henyey_greenstein_phase) // 太陽光位相関数
                    ) 
                    * lerp(0.0f, 1.0f, up_light * top_light)
                    + Ei;

                    // 合計ライト
                    float3 lighting=0.0f;
                    {
                        float view_zenith = saturate(dot(ray_direction, float3(0, 1, 0))); // 視線の上下

                        // 横方向ほど 1 に近づく
                        float lateral = 1.0 - view_zenith;

                        // Beer を“平方根側”に寄せる
                        float beer_lateral =  lerp(beers_law, sqrt(beers_law), lateral * 0.7);

                        //float shadow = pow(beer_lateral,1.2);
                        float shadow = beer_lateral * sqrt(beer_lateral);
                        lighting =
                        direct_light * shadow +
                        atmosphere_color * lerp(0.50f, shadow, 0.2f);
                    }

                    // 局所的な消光係数（extinction）； density_scale を光学係数として利用
                    float sigma = density_scale * sampled_density;

                    // そのステップでのトランスミッタンス変化（解析積分）
                    //float T_step = exp(-sigma * step_size);
                    float T_step = exp2((-sigma * step_size)*1.44269504);

                    // ステップ内で散乱/吸収された割合（フラクション）
                    float delta = transmittence * (1.0f - T_step);

                    // サンプルの光寄与を追加（phaseやpowdered_sugarなどのスケールは lighting に含めてもよい）
                    color += lighting * delta;

                    // トランスミッタンスを更新
                    transmittence = transmittence * T_step;
                    //視線方向の透過率を更新
                    
                    
                    //早期終了判定
                    if (transmittence <= 1e-6f)
                    {
                        break;
                    }
                    
                }
                
            }
            //六回連続０なら雲を抜けた元して判断する
            else
            {
                cloud_test = 0.0;
                zero_density_sample_count = 0;
            }
                //サンプル位置を進める
                sample_point +=ray_step;
        }
        else
        {
			//雲の外判定中は低品質サンプリング
            float3 weather_data = SampleWeatherData(sample_point.xz);
            
            cloud_test = 
            SampleCloudDensity(sample_point, weather_data, 6.0, true /*cheap_sample*/);
            float step_scale = 1.0f;
            if (cloud_test <= 0.0)
            {
                //地平線は荒く、上空は細かくステップを進める
                step_scale = lerp(1.0f, 3.0f, horizon);
            }
            sample_point += ray_step * step_scale;
        }
        
        
        dist = length(sample_point - ray_origin);
        if(dist>=limit_dist)
        {
            break;
        }

    }    
        	    
    float alpha = saturate(1.0 - transmittence);
    
    return max(0.0, float4(color, alpha));
}

// ピクセルシェーダーメイン関数
float4 main(VS_OUT pin) : SV_TARGET
{
    float4 ndc = float4(2.0 * pin.texcoord.x - 1.0, 1.0 - 2.0 * pin.texcoord.y, 0.0, 1.0);
    float4 pos = mul(ndc, inverse_view_projection_transform);
    pos /= pos.w;

    // 地球中心基準のカメラ位置
    float3 ray_dir = normalize(pos.xyz - camera_position.xyz);

    float3 color = 0.0;
    //地平線上
    float3 eye_pos = float3(0.0, earth_radius, 0.0) + camera_position.xyz;
    
    float t0 = IntersectSphere(eye_pos, ray_dir, cloud_altitudes_min_max.x);
    float t1 = IntersectSphere(eye_pos, ray_dir, cloud_altitudes_min_max.y);
    
    //交差しない場合、雲を描画させない
    if(t0<0.0f || t1<0.0f || t1<=t0)
    {
        clip(-1);
    }
    
    float3 ray_origin = eye_pos + ray_dir * t0;
    float3 ray_endpoint = eye_pos + ray_dir * t1;
    float shell_dist = length(ray_endpoint - ray_origin);
    
    if (ray_dir.y>=0.0f)
    {

        float steps = ray_marching_steps;
        
        //自動ステップ調整    
        if (auto_ray_marching_steps)
        {
            steps = lerp(
            ray_marching_steps * 0.5625, 
            ray_marching_steps,
            clamp(dot(ray_dir, float3(0.0, 1.0, 0.0)), 0.0, 1.0)
            );
            {
                float viewUp = saturate(ray_dir.y);
                float horizon = 1.0 - viewUp;

                steps = lerp(
                ray_marching_steps * 0.3,
                ray_marching_steps,
                smoothstep(0.0, 0.6, viewUp)
                );
            }
        }

        float3 ray_step = ray_dir * shell_dist / steps;
        // メインのレイマーチング関数を呼び出す
        float4 volume = RayMarch(ray_origin, ray_step, int(steps),pin.texcoord.xy);
        //空の色
        float3 background = sky_color_texture.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord.xy).rgb;
        // 雲のボリュームカラーと背景色をアルファブレンド
        color = (background * (1.0 - volume.a)) + volume.xyz;
        
        // 遠くの雲を空にブレンド
        {
            // 地平線方向でフェードを強める（任意だが非常に効く）
            float view_up = saturate(dot(ray_dir, float3(0, 1, 0)));
            float horizon = 1.0 - view_up;
            float horizon_fade = ((horizon * horizon) * (horizon * horizon));
            
            //雲の色を空に近づけるが、遮蔽は意地
            float3 cloud_color_only =
            lerp(volume.rgb, background * volume.a, horizon_fade);
            
            color = background * (1.0f - volume.a) + cloud_color_only;
            
        }

    }
    else
    {
        //地平線より下は雲を描画しない
        color = sky_color_texture.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord.xy).rgb;
    }


    return float4(color, 1.0f);

}