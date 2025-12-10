//#include"cloud_dome.hlsli"
#include"forward_light.hlsli"
#include "scene_constant_buffer.hlsli"
//#include "noise_functions.hlsli"
#include "fullscreen_quad.hlsli"

cbuffer CLOUD_RAY_MARCHING_CONSTNAT_BUFFER : register(b12)
{
    float2 wind_direction;
    float2 cloud_altitudes_min_max; // highest and lowest altitudes at which clouds are distributed
	
    float wind_speed; // [0.0, 20.0]
	
    float density_scale; // [0.01, 0.2]
    float cloud_coverage_scale; // [0.1, 1.0]
    float rain_cloud_absorption_scale;
    float cloud_type_scale;

    float earth_radius; // earth radius
    float horizon_distance_scale;
    float low_frequency_perlin_worley_sampling_scale;
    float high_frequency_worley_sampling_scale;
    float cloud_density_long_distance_scale;
    bool enable_powdered_sugar_efffect;
	
    uint ray_marching_steps;
    bool auto_ray_marching_steps;
};

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
Texture3D<float4> mid_frequency_worley_texture : register(t2);
Texture2D<float3> weather_texture : register(t3);
Texture2D<float3> curl_noise_texture : register(t4);
#if 0
Texture2D<float> gradient_stratus_texture : register(t4);
Texture2D<float> gradient_cumulus_texture : register(t5);
Texture2D<float> gradient_cumulonimbus_texture : register(t6);
#endif

static const float time_offset = 10000.0;
float4 SampleLowFrequencyNoises(float3 sample_point, float mip_level)
{
    return low_frequency_perlin_worley_texture.SampleLevel(sampler_states[LINEAR_MIRROR], sample_point * low_frequency_perlin_worley_sampling_scale, mip_level);
}
float3 SampleHighFrequencyNoises(float3 sample_point, float mip_level)
{
    return high_frequency_worley_texture.SampleLevel(sampler_states[LINEAR_MIRROR], sample_point * high_frequency_worley_sampling_scale, mip_level);
}
float4 SampleMidFrequencyNoises(float3 sample_point,float mip_level)
{
    return mid_frequency_worley_texture.SampleLevel(sampler_states[LINEAR_MIRROR], sample_point * low_frequency_perlin_worley_sampling_scale, mip_level);
}
float3 SampleWeatherData(float2 sample_point)
{
    float2 offset = ((options.z + time_offset) * 0.001) * normalize(-wind_direction) * wind_speed;

#if 1
    float horizon_distance = sqrt(cloud_altitudes_min_max.x * cloud_altitudes_min_max.x - earth_radius * earth_radius) * horizon_distance_scale;
    return weather_texture.Sample(sampler_states[LINEAR_MIRROR], float2(sample_point.x + horizon_distance, horizon_distance - sample_point.y) / (2.0 * horizon_distance) + offset);
#else
	const float weather_scale = 0.00006;
	return weather_texture.SampleLevel(sampler_states[LINEAR_MIRROR], sample_point * weather_scale + 0.5 + offset, 0);
#endif
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
    return new_min + (((original_value - original_min) / (original_max - original_min)) * (new_max - new_min));
}


static const float RAYLEIGH_SCALE_HEIGHT = 8000.f;
static const float MIE_SCALE_HEIGHT = 1200.f;
static const float OZONE_SCALE_HALF_WIDTH = 15000.f;
static const float OZONE_CENTER_HEIGHT = 50000.f;
static const float EARTH_RADIUS = 6360000.0f; // 地球半径 [m]
static const float SUN_DISTANCE = 150000000000.0f; // 太陽までの距離 [m]
static const float ATMOSPHERE_HEIGHT = 100000.0f; // 大気の高さ [m]
static const int MAX_SAMPLES = 16;

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
//フェーズ関数(レイリー散乱)
float RayleighPhase(float cos_theta)
{
    return (3.0f * (1.0f + cos_theta * cos_theta))
            / (16.0 * PI);
    
    //return (3.0 / (16.0 * PI)) * (1.0 + pow(cos_theta * 0.5 + 0.5, 2.0));//福井先生のコードより
}
//フェーズ関数(ミー散乱)
float MiePhase(float cos_theta, float g)
{
    //ヘニエイ・グリーンスタイン関数
    float g2 = g * g;
    
    return (1.0f - g2) / (pow(1.0f + g2 - 2.0f * g * cos_theta, 1.50f) * 4.0f * PI);
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

//複数回散乱して入る入散乱の光を計算する
float3 PrecomputeMultiScattering(float3 position /*地球半径加算済み*/, float3 view_dir, float3 light_dir)
{
    float3 sample_pos = position;
    //高度計算
    float h = length(sample_pos) - EARTH_RADIUS; //自身の位置-地球半径
    //100メートル地下ならば、計算する必要はない
    if (h < -100.f)
        return float3(0.f, 0.f, 0.f);
    
    //散乱係数
    float3 sigma_s = SigmaRayleigh(h) + SigmaMie(h);
    float3 sigma_t = sigma_s + SigmaOzone(h);
    
    //太陽光スペクトルの減衰(波長依存)
    float sun_elevation = clamp(dot(light_dir, float3(0, 1, 0)), 0.0f, 1.0f); // 太陽の高さ
    float sun_theta = acos(sun_elevation) * (180.0f / PI); //度に変換
    //kasten-Young 1989近似
    float air_mass = 1.0f / (sun_elevation + (0.50572f * pow(96.07995 - sun_theta, -1.6364))); // secant近似
    air_mass = min(air_mass, 20.0f); // 極端な値を制限
    float3 Ei = ComputeSunIrradiance(air_mass);
    
    //位相関数(散乱光の内こちらに向く割合)
    float horizon_factor = saturate(1.0f - dot(view_dir, float3(0, 1, 0))); //０＝天頂、1＝地
    float mie_boost = 0.5f * horizon_factor * (0.01f + air_mass * 0.05f);
    
    float cos_theta = clamp(dot(view_dir, light_dir), -0.4f, 1.0f);
    float sunset_factor = saturate((air_mass - 1.0f) / 20.0f); //1~10を0～1に正規化
    float phase = ((RayleighPhase(sun_elevation) * (lerp(10.0f, 2.f, sunset_factor))
    + MiePhase(sun_elevation, 0.5) * mie_boost) / (4.0f * PI));
    
    //太陽方向
    float3 T1 = TransmittanceApprox(sample_pos, sample_pos + light_dir * ATMOSPHERE_HEIGHT);
    //中継点方向
    float3 T2 = TransmittanceApprox(position, sample_pos);
    
    //光学的厚さ τを簡易的に推定
    //積分をしたくないので設定
    float approx_sun_distance = ATMOSPHERE_HEIGHT; //大気圏厚み
    float sigma_t_avg = (sigma_t.x + sigma_t.y + sigma_t.z) / 3.0f; //チャンネル平均
    float tau_sun = min(sigma_t_avg * approx_sun_distance, 2.0f); //単純近似の光学的深さ
    
    //単一散乱寄与
    //ここでサンプル店の寄与量を調整するために、stepLength  掃討のスケールをかける
    float step_scale = approx_sun_distance * 0.5f;
    float3 L2 = T1 * sigma_s * phase * Ei * step_scale;
    
    //多重散乱係数f_ms
    //f_ms=omega_avg*(1-exp(-tau_sun))： τ が大きいほど多重散乱が増える
    float3 omega = sigma_s / max(sigma_t, float3(1e-6, 1e-6, 1e-6)); // アルベド（散乱/全係数）
    float omega_avg = (omega.x + omega.y + omega.z) / 3.0;
    float f_ms = saturate(omega_avg * (1.0f - exp(-tau_sun)));
    //F_ms：漸か式近似1/(1-f_ms)
    float F_ms = 1.0 / max(1.0 - f_ms, 1e-3f);

    //最終psiに対してT2をかけて視線上の減衰を考慮
    float3 psi_ms = L2 * F_ms;
    return psi_ms;
}

//散乱光の計算(シングルスキャッタリング＋その地点までの散乱光)
float3 ComputeSkyColor(float3 camera_pos, float3 view_dir, float3 light_dir)
{
    
    float cos_theta = clamp(dot(view_dir, light_dir), -0.4f, 1.0f); //視線と太陽の角度
    float sun_elevation = clamp(dot(light_dir, float3(0, 1, 0)), 0.0f, 1.0f); // 太陽の高さ
    float sun_theta = acos(sun_elevation) * (180.0f / PI); //度に変換
    //kasten-Young 1989近似
    float air_mass = 1.0f / (sun_elevation + (0.50572f * pow(96.07995 - sun_theta, -1.6364))); // secant近似
    air_mass = min(air_mass, 50.0f); // 極端な値を制限

    float sunset_factor = saturate((air_mass - 1.0f) / 20.0f); //1~10を0～1に正規化
    float3 Ei = ComputeSunIrradiance(air_mass);
    
    
    //太陽に近いほど1.0
    float angle_factor = pow(saturate(cos_theta), 2.0f);
    float phase_rayliegh = RayleighPhase(cos_theta) * (lerp(30.0f, 10.f, sunset_factor));
    //夕焼けを作る際、夕焼けは太陽の傾きによるミー散乱の強化が主な要因の為、
    //太陽の傾きで強くする
    float horizon_factor = saturate(1.0f - dot(view_dir, float3(0, 1, 0)));
    float mie_boost = 0.001f + horizon_factor * (0.01f + air_mass * 0.05f);
    float phase_mie = MiePhase(cos_theta, 0.8f) * mie_boost;
    
    //青空の時は変化が少ないのでサンプル数を減らし、
    //変化の多い地平線付近だけ多めにする
    float samples_f = lerp(MAX_SAMPLES / 2, (float) MAX_SAMPLES, horizon_factor);
    int adaptive_samples = max(1, (int) round(samples_f)); // round() -> float, cast -> int

    float step_size = ATMOSPHERE_HEIGHT / adaptive_samples;
    float3 result = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < adaptive_samples; ++i)
    {
        float u = (i + 0.5f) / adaptive_samples;
        float t = ATMOSPHERE_HEIGHT * (1.0f - pow(1.0f - u, 2.0f)); // 下層に多く
        //float t = i * step_size;

        float3 sample_pos = camera_pos + view_dir * t;
        float h = length(sample_pos) - EARTH_RADIUS;

        //100メートル地下ならば、考慮に入れなくても良い
        if (h < 0.0f)
        {
            continue;
        }
        
        float3 sigma_s = SigmaRayleigh(h) + SigmaMie(h);
        float3 T1 = TransmittanceApprox(camera_pos, sample_pos);
        float3 T2 = TransmittanceApprox(sample_pos, sample_pos + light_dir * ATMOSPHERE_HEIGHT);
        
        result += T1 * sigma_s * ((phase_rayliegh + phase_mie)) * T2 * Ei * step_size;
    }
        //太陽
    {
     //   const float sol_size = 0.00872663806;
     //   const float sun_disk_scale = 2.0; // [0.0, 360.0]
	    //// solar disk and out-scattering
     //   float sun_angular_diameter_cos_max = cos(sol_size * sun_disk_scale * 0.5);
     //   float sun_angular_diameter_cos_min = cos(sol_size * sun_disk_scale);
        
     //   float sun_elevation = clamp(dot(light_dir, float3(0, 1, 0)), 0.0f, 1.0f); // 太陽の高さ
     //   float sun_theta = acos(sun_elevation) * (180.0f / PI); //度に変換
        
     //   float sun_disk = lerp(sun_angular_diameter_cos_min, sun_angular_diameter_cos_max, cos_theta);
     //   float3 Lo = sun_disk * Ei * directional_light.intensity;
     //   // 太陽のディスク内に視線が入っているときだけ加算
     //   if (sun_disk > 0.01f) // しきい値で完全に限定
     //   {
     //       result += Lo*0.01f;
     //   }
    }
    return result;
}

// fractional value for sample position in the cloud layer
float GetHeightFractionForPoint(float position)
{
	// get global fractional position in cloud zone
    float height_fraction = (position - cloud_altitudes_min_max.x) / (cloud_altitudes_min_max.y - cloud_altitudes_min_max.x);
    return clamp(height_fraction, 0.0, 1.0);
}

float4 MixGradients(float cloud_type)
{
    const float4 stratus_gradient = float4(0.02f, 0.05f, 0.09f, 0.11f);
    const float4 stratocumulus_gradient = float4(0.02f, 0.2f, 0.48f, 0.625f);
    const float4 cumulus_gradient = float4(0.01f, 0.0625f, 0.78f, 1.0f);
	
    float stratus = 1.0f - clamp(cloud_type * 2.0f, 0.0, 1.0);
    float stratocumulus = 1.0f - abs(cloud_type - 0.5f) * 2.0f;
    float cumulus = clamp(cloud_type - 0.5f, 0.0, 1.0) * 2.0f;
	
    return stratus_gradient * stratus + stratocumulus_gradient * stratocumulus + cumulus_gradient * cumulus;
}

float GetDensityHeightGradient(float height_fraction, float cloud_type)
{
    float density_gradient = 0.0;

#if 0
	const float stratus_threshold = 0.1;
	const float stratocumulus_threshold = 0.9;
	//const float cumulus_threshold = 1.0;

	int type = 2;
    // cloud type: {0: stratus, 1: cumulus, 2: cumulonimbus}
	if (cloud_type < stratus_threshold)
	{
		type = 0;
	}
	else if (cloud_type < stratocumulus_threshold)
	{
		type = 1;
	}
	
	
	// sample from gradient texture
	if (type == 0) // stratus clouds
	{
		density_gradient = gradient_stratus_texture.SampleLevel(sampler_states[LINEAR_BORDER_BLACK], float2(0.5, 1.0 - height_fraction), 0);
	}
	else if (type == 1) // cumulus clouds
	{
		density_gradient = gradient_cumulus_texture.SampleLevel(sampler_states[LINEAR_BORDER_BLACK], float2(0.5, 1.0 - height_fraction), 0);
	}
	else if (type == 2)// cumulunimbusclouds
	{
		density_gradient = gradient_cumulonimbus_texture.SampleLevel(sampler_states[LINEAR_BORDER_BLACK], float2(0.5, 1.0 - height_fraction), 0);
	}
#else
    const float4 stratus_gradient = float4(0.02f, 0.05f, 0.09f, 0.11f);
    const float4 stratocumulus_gradient = float4(0.02f, 0.2f, 0.48f, 0.625f);
    const float4 cumulus_gradient = float4(0.01f, 0.0625f, 0.78f, 1.0f);
	
    float stratus = 1.0f - clamp(cloud_type * 2.0f, 0.0, 1.0);
    float stratocumulus = 1.0f - abs(cloud_type - 0.5f) * 2.0f;
    float cumulus = clamp(cloud_type - 0.5f, 0.0, 1.0) * 2.0f;

    float4 cloud_gradient = stratus_gradient * stratus + stratocumulus_gradient * stratocumulus + cumulus_gradient * cumulus;
    density_gradient = smoothstep(cloud_gradient.x, cloud_gradient.y, height_fraction) - smoothstep(cloud_gradient.z, cloud_gradient.w, height_fraction);
#endif
    return density_gradient;

}

float IntersectSphere(float3 pos, float3 dir, float r)
{
	// define the center of the sphere as the origin of coordinates
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, pos);
    float c = dot(pos, pos) - (r * r);
    float d = sqrt((b * b) - 4.0 * a * c);
    float p = -b - d;
    float p2 = -b + d;
    return max(p, p2) / (2.0 * a);
}

#define ENABLE_CLOUD_ANIMATION 
// Andrew Schneider "Real-Time Volumetric Cloudscape" In GPU Pro 7, pp. 97-127
// returns density at a given point
float SampleCloudDensity(float3 sample_point, float3 weather_data, float mip_level, bool cheap_sample = false)
{
	// get the height-fraction for use with blending noise types over height
    float height_fraction = GetHeightFractionForPoint(length(sample_point));
	
#ifdef ENABLE_CLOUD_ANIMATION
    sample_point.xz += (options.z + time_offset) * 20.0 * normalize(-wind_direction) * wind_speed * 0.6;
#endif
	
	// read the low-frequency perlin-worley and worley noises
    float4 low_frequency_noises = SampleLowFrequencyNoises(sample_point, mip_level - 2.0);

	// build an fbm(fractal brownian motion) out of the low-frequency worley noises that can be used to add detail to the low-frequency perlin-worley noise
    float low_frequency_fbm = low_frequency_noises.g * 0.625 + low_frequency_noises.b * 0.25 + low_frequency_noises.a * 0.125;
    
	// define the base cloud shape by dilating it with the low-frequency fbm(fractal brownian motion) mode of worley noise
    float base_cloud = Remap(low_frequency_noises.r, -(1.0 - low_frequency_fbm), 1.0, 0.0, 1.0);
	
	// get the density-height gradient using the density height function
    float cloud_type = clamp(weather_data.b * cloud_type_scale, 0.0, 1.0);
    float density_height_gradient = GetDensityHeightGradient(height_fraction, cloud_type);
	
	// apply the height function to the base cloud shape
    base_cloud *= density_height_gradient;
	
	// cloud coverage is stored in weather_data's red channel
    float cloud_coverage = weather_data.r * cloud_coverage_scale;
	
	// use remap to apply the cloud  coverage attribute
    float base_cloud_with_coverage = Remap(base_cloud, 1.0 - cloud_coverage, 1.0, 0.0, 1.0);
    base_cloud_with_coverage *= cloud_coverage;

    float final_cloud = base_cloud_with_coverage;
    if (!cheap_sample && base_cloud_with_coverage > 0.0)
    {
		// constract with worley noise at the top produces billowing detail
	
#ifdef ENABLE_CLOUD_ANIMATION
        if (wind_speed != 0.0)
        {
            sample_point.xz -= (options.z + time_offset) * normalize(-wind_direction) * 40.0;
            sample_point.y -= (options.z + time_offset) * 40.0;
        }
#endif
		
#if 1
		// add some turbulence to bottom of clouds
        float3 curl_noise = curl_noise_texture.SampleLevel(sampler_states[LINEAR_MIRROR], sample_point.xy * 0.00008, 0);
        sample_point.xy = sample_point.xy + curl_noise.xy * (1.0 - height_fraction);
#endif
		
		// sample high-frequency noises
        float3 high_frequency_noises =SampleHighFrequencyNoises(sample_point, mip_level);
	
		// build high-frequency worley noise fbm(fractal brownian motion)
        float high_frequency_fbm = high_frequency_noises.r * 0.625 + high_frequency_noises.g * 0.25 + high_frequency_noises.b * 0.125;
	
		// transition from wispy shape to billowy shape over height
#if 1
		float high_frequency_noise_modifier = lerp(high_frequency_fbm, 1.0 - high_frequency_fbm, clamp(height_fraction * 10.0, 0.0, 1.0));
#else
        float high_frequency_noise_modifier = lerp(high_frequency_fbm, 1.0 - high_frequency_fbm, clamp(height_fraction * 4.0, 0.0, 1.0));
#endif
	
		// erode the base cloud shape with distorted high-frequency worley noises
#if 1
        final_cloud = Remap(base_cloud_with_coverage, high_frequency_noise_modifier * 0.2, 1.0, 0.0, 1.0);
#else	
		final_cloud = remap(base_cloud_with_coverage, high_frequency_noise_modifier * 0.4 * height_fraction, 1.0, 0.0, 1.0);
#endif
    }

#if 0
	return clamp(final_cloud, 0.0, 1.0);
#else
    return pow(clamp(final_cloud, 0.0, 1.0), (1.0 - height_fraction) * 0.8 + 0.5);
#endif
}

static const float3 noise_kernel[6] =
{
    float3(0.38051305f, 0.92453449f, -0.02111345f),
    float3(-0.50625799f, -0.03590792f, -0.86163418f),
    float3(-0.32509218f, -0.94557439f, 0.01428793f),
    float3(0.09026238f, -0.27376545f, 0.95755165f),
    float3(0.28128598f, 0.42443639f, -0.86065785f),
    float3(-0.16852403f, 0.14748697f, 0.97460106f)
};
// a function to gather density in a cone for use with lighting clouds
float SampleCloudDensityAlongCone(float3 ray_origin, float3 ray_direction /*normalized*/, float cone_spread_multplier /*how wide to make the cone*/)
{
	
    float3 sample_point = ray_origin;
	
    float3 weather_data = 0.0;
    float density_along_cone = 0.0;
	
	// lighting ray-march loop
	[unroll]
    for (int i = 0; i < 6; i++)
    {
		// add the current step offset to the sample position
        sample_point += (ray_direction + noise_kernel[i] * float(i)) * cone_spread_multplier;
        weather_data = SampleWeatherData(sample_point.xz);
#if 0
		// sample cloud density the cheap way, using only one level of noise
		density_along_cone += sample_cloud_density(sample_point, weather_data, float(i) /*mip_level*/, density_along_cone > 0.3 /*cheap_sample*/);
#else
		// always sample cloud density the expensive way
        density_along_cone += SampleCloudDensity(sample_point, weather_data, float(i) /*mip_level*/, false /*cheap_sample*/);
#endif
    }

    return density_along_cone;
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

float4 RayMarch(float3 ray_origin, float3 ray_step, int steps)
{
    float step_size = length(ray_step);
	
	// レイ開始位置を少しランダムにして横筋を目立たなくする（ディザリング）
    float3 sample_point = ray_origin + ray_step * Hash(ray_origin * 10.0); // dithering ? 
	
    //太陽方向と位相関数
    float3 sun_direction = normalize(-directional_light.direction.xyz);
    float cos_theta = dot(sun_direction, -normalize(ray_step));
    float g = 0.6f;
    //float henyey_greenstein_phase = max(max(MiePhase(cos_theta, 0.4f), MiePhase(cos_theta, (0.4 - 1.4 * sun_direction.y))), MiePhase(cos_theta, -0.2)); // TODO
    float henyey_greenstein_phase = MiePhase(cos_theta, g);
    henyey_greenstein_phase = saturate(henyey_greenstein_phase); //0~1.0の範囲に圧縮

    
	// precalculate atmosphere color
    float3 position = float3(0.f, EARTH_RADIUS, 0.f);
    //カメラから天球の各頂点への方向
    
    //大気散乱の事前計算
    //疑似多重散乱の事前計算
    float3 multi_scattering = PrecomputeMultiScattering(position, ray_step, sun_direction);
    ////シングルスキャッタリング
    float3 single_scattering = ComputeSkyColor(
    position,
    ray_step,
    sun_direction);
    float3 atmosphere_color = (multi_scattering + single_scattering)*step_size*0.1f;
	
    const float cone_spread_multplier = ((cloud_altitudes_min_max.y - cloud_altitudes_min_max.x) / 36.0); // how wide to make the cone
	
    //水平方向の光の制御
    float horizon_factor = saturate(dot(ray_step, float3(0, 1, 0)));
    
    float3 color = 0.0;
    float density = 0;
    float transmittence = 1.0;
    float cloud_test = 0;
    int zero_density_sample_count = 0;
	
	// start the main ray-march loop
	[loop]
    for (int i = 0; i < steps; i++)
    {
        //雲内部なら高品質
        if (cloud_test > 0.0)
        {
			// sample density the expensive way the setting the last parameter to false, indicating a full sample
            float3 weather_data = SampleWeatherData(sample_point.xz);
            float sampled_density = SampleCloudDensity(sample_point, weather_data, 0.0, false /*cheap_sample*/);
			// if we just sample a zero, increment the counter
            if (sampled_density == 0.0)
            {
                zero_density_sample_count++;
            }
			// if we are doing an expensive sample that is still potentially in the cloud
            if (zero_density_sample_count != 6)
            {
                density += sampled_density;
                if (sampled_density != 0.0)
                {
					// the accumulating variable for transmittence
                    transmittence *= exp(-density_scale * sampled_density * step_size ); // 減衰を弱める
                    
					// sample_cloud_density_along_cone just walks in the given direction from the start point and takes 6 number of lighting samples
                    float density_along_light_ray = 0.0;
                    density_along_light_ray += SampleCloudDensityAlongCone(sample_point, sun_direction, cone_spread_multplier);
                    density_along_light_ray += SampleCloudDensityLongDistance(sample_point, sun_direction, cone_spread_multplier);
			
					// captures the direct lighting from the sun
                    float optical_thickness = density_scale * density_along_light_ray * cone_spread_multplier;
                    //水平方向の吸収を緩和する
                    //optical_thickness *= lerp(0.5, 1.0, horizon_factor);
                    float rain_cloud_absorption = exp(-weather_data.g) * rain_cloud_absorption_scale;
					// beer's law models the attenuation of light as it passes through a material
#if 1
                    float beers_law = exp(-optical_thickness * rain_cloud_absorption);
#else
					float beers_law = max(exp(-optical_thickness * rain_cloud_absorption), exp(-optical_thickness * 0.25 * rain_cloud_absorption) * 0.7);
#endif
					// 雲内部を明るくする近似
                    float powdered_sugar = enable_powdered_sugar_efffect ? 1.0 - exp(-optical_thickness) : 0.5;
					
                    // 太陽直接光
                    float3 sun_light_color = float3(10.0, 9.5, 9.f); // or passed from CPU
                    float3 direct_light =
                    2.0f*
                    beers_law *
                    powdered_sugar *
                    henyey_greenstein_phase +
                    sun_light_color;

                    // 合計ライト
                    float3 lighting = direct_light + atmosphere_color; //多重散乱ブーストとして

                    // 雲色寄与
                    color += lighting * sampled_density * transmittence;

                }
                sample_point += ray_step;
            }
            //六回連続０なら雲を抜けた元して判断する
            else
            {
                cloud_test = 0.0;
                zero_density_sample_count = 0;
            }
        }
        else
        {
			// sample density the cheap way, only using the low-frequency noise 
            float3 weather_data = SampleWeatherData(sample_point.xz);
            cloud_test = SampleCloudDensity(sample_point, weather_data, 0.0, true /*cheap_sample*/);
            if (cloud_test == 0.0)
            {
                sample_point += ray_step;
            }
        }
    }
    
    color /= (color + 1.0f);
	
    float alpha = 1.0 - transmittence;
    return max(0.0, float4(color, alpha));
}


float4 main(VS_OUT pin) : SV_TARGET
{
    float4 ndc = float4(2.0 * pin.texcoord.x - 1.0, 1.0 - 2.0 * pin.texcoord.y, 0.0, 1.0);
    float4 pos = mul(ndc, inverse_view_projection_transform);
    pos /= pos.w;

    float3 ray_dir = normalize(pos.xyz - camera_position.xyz);
    float3 position = float3(0.f, EARTH_RADIUS, 0.f);
    float3 light_dir = normalize(-directional_light.direction.xyz);
    float3 sun_pos = light_dir * SUN_DISTANCE;
    float3 sun_dir = normalize(sun_pos - camera_position.xyz);

    float3 multi_scattering = PrecomputeMultiScattering(position, ray_dir, sun_dir);
    float3 single_scattering = ComputeSkyColor(position, ray_dir, sun_dir);

    float3 color = 0.0;
    if (ray_dir.y > 0.0)
    {
        float3 eye_pos = float3(0.0, earth_radius, 0.0) + camera_position.xyz;
        float3 ray_origin = eye_pos + ray_dir * IntersectSphere(eye_pos, ray_dir, cloud_altitudes_min_max.x);
        float3 ray_endpoint = eye_pos + ray_dir * IntersectSphere(eye_pos, ray_dir, cloud_altitudes_min_max.y);
        float shell_dist = length(ray_endpoint - ray_origin);

        float steps = ray_marching_steps;
        if (auto_ray_marching_steps)
        {
            steps = lerp(ray_marching_steps * 0.5625, ray_marching_steps, clamp(dot(ray_dir, float3(0.0, 1.0, 0.0)), 0.0, 1.0));
        }

        float3 ray_step = ray_dir * shell_dist / steps;
        float4 volume = RayMarch(ray_origin, ray_step, int(steps));

        //float3 background = atmosphere(ray_dir);
        float3 background = multi_scattering + single_scattering;
		// draw cloud shape
        color = background * (1.0 - volume.a) + volume.xyz;
		
		// blend distant clouds into the sky
        color = lerp(clamp(color, 0.0, 1.0), clamp(background, color, 1.0), smoothstep(0.6, 1.0, 1.0 - ray_dir.y));

    }
    else
    {
        color = multi_scattering + single_scattering;
        color = lerp(color * 0.5f, float3(0, 0, 0), -ray_dir.y);
    }

    return float4(color, 1.0f);
}
