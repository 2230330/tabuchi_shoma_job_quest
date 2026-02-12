#include"sky_atmosphere.hlsli"
#include"scene_constant_buffer.hlsli"
#include"forward_light.hlsli"

Texture2D sky_texture : register(t0);

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
SamplerState sampler_states[5] : register(s0);

static const float PI = 3.14159265358979323846f;

static const float RAYLEIGH_SCALE_HEIGHT = 8000.f;
static const float MIE_SCALE_HEIGHT = 1200.f;
static const float OZONE_SCALE_HALF_WIDTH = 15000.f;
static const float OZONE_CENTER_HEIGHT = 50000.f;
static const float EARTH_RADIUS = 6360000.0f; // 地球半径 [m]
static const float SUN_DISTANCE = 150000000000.0f; // 太陽までの距離 [m]
static const float ATMOSPHERE_HEIGHT = 100000.0f; // 大気の高さ [m]
static const int MAX_SAMPLES = 64;

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
    float ozoneFactor = max(0.0, 1.0 - (abs(h-OZONE_CENTER_HEIGHT)/OZONE_SCALE_HALF_WIDTH));
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
float MiePhase(float cos_theta,float g)
{
    //ヘニエイ・グリーンスタイン関数
    float g2 = g * g;
    return (1.0f - g2) / (pow(1.0f + g2 - 2.0f * g * cos_theta, 1.50f) * 4.0f * PI);
}

//距離×平均係数で指数減衰を近似
float3 TransmittanceApprox(float3 startPos, float3 endPos)
{
    float distance = max(50e3f,length(endPos - startPos));
    // 高度に依存した sigma_t_avg を評価するのが理想だが、まずは sample midpoint を使う
    float3 midPos = (startPos + endPos) * 0.5f;
    float h = max(0.0f,length(midPos) - EARTH_RADIUS);
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
    float3 tau = INV_WAVELENGTH4 * air_mass *3e9f /*スキャッタリングスケール*/;

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
    //air_mass = min(air_mass, 20.0f); // 極端な値を制限
    float3 Ei = ComputeSunIrradiance(air_mass);
    
    //位相関数(散乱光の内こちらに向く割合)
    float horizon_factor = saturate(1.0f - dot(view_dir, float3(0, 1, 0))); //０＝天頂、1＝地
    float mie_boost = 0.5f * horizon_factor * (0.01f + air_mass * 0.05f);
    
    float cos_theta = lerp(dot(view_dir, light_dir), -.4f, 1.0f);
    float sunset_factor = saturate((air_mass - 1.0f) / 20.0f); //1~10を0～1に正規化
    float phase = ((RayleighPhase(cos_theta) * (lerp(10.0f, 2.f, sunset_factor))
    + MiePhase(cos_theta, 0.5) * mie_boost) / (4.0f * PI));
    
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
    //air_mass = min(air_mass, 50.0f); // 極端な値を制限

    float sunset_factor = saturate((air_mass - 1.0f) / 10.0f); //1~10を0～1に正規化
    float3 Ei = ComputeSunIrradiance(air_mass);
    
    //太陽に近いほど1.0
    float angle_factor = pow(saturate(cos_theta), 2.0f);
    float phase_rayliegh = RayleighPhase(cos_theta) * (lerp(20.0f, 10.f, sunset_factor));
    //夕焼けを作る際、夕焼けは太陽の傾きによるミー散乱の強化が主な要因の為、
    //太陽の傾きで強くする
    float horizon_factor = saturate(1.0f - dot(view_dir, float3(0, 1, 0)));
    float mie_boost =horizon_factor * (0.01f + air_mass * 0.05f);
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
        if (h < -100.0f)
        {
            continue;
        }
        
        float3 sigma_s = SigmaRayleigh(h) + SigmaMie(h);
        float3 T1 = TransmittanceApprox(camera_pos, sample_pos);
        float3 T2 = TransmittanceApprox(sample_pos, sample_pos + light_dir * ATMOSPHERE_HEIGHT);
        
        result += T1 * sigma_s * ((phase_rayliegh + phase_mie)) * T2 * Ei * step_size;
    }
    
    
    return result;
}

//大気散乱
float4 main(VS_OUT pin):SV_TARGET
{    
    float4 color = sky_texture.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
  
    //大気散乱    
    float3 sky_color = float3(0, 0, 0);

    float3 position =  float3(0.f, height+EARTH_RADIUS, 0.f);
    float3 light_dir = normalize(-directional_light.direction.xyz);
    float3 sun_pos = light_dir * SUN_DISTANCE; //太陽の位置()
    float3 sun_dir = normalize(sun_pos.xyz-pin.world_pos.xyz);//頂点ー＞太陽
    //カメラから天球の各頂点への方向
    float3 view_dir = normalize(pin.world_pos.xyz - camera_position.xyz); //camera->頂点まで方向
    
    //疑似多重散乱の事前計算
    float3 multi_scattering = PrecomputeMultiScattering(position, view_dir, sun_dir);
    
    ////シングルスキャッタリング
    float3 single_scattering = ComputeSkyColor(
    position,
    view_dir,
    sun_dir);
    
    sky_color += single_scattering + (multi_scattering) ; //環境光的に少し足す
    
    //夜の簡易実装
    float cos_theta = clamp(dot(view_dir, light_dir), -1.0f, 1.0f); //視線と太陽の角度
    float sun_elevation = clamp(dot(light_dir, float3(0, 1, 0)), 0.0f, 1.0f); // 太陽の高さ

    sky_color += lerp(float3(0.1f, 0.1f, 0.2f), 0, sun_elevation);

    return float4(sky_color.xyz, 1.0f);
}