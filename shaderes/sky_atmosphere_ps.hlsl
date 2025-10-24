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

#define PI 3.14159265358979323846

//簡易的レイリー散乱
float3 SigmaRayleigh(float h)
{
    
    return float3(5.802e-6, 13.558e-6, 33.1e-6) * exp(-(h) / 8400.0);
}


//簡易的ミー散乱
float3 SigmaMie(float h)
{
    return float3(3.996e-6, 4.40e-6, 0.0) * exp(-(h) / 1250.0);
}


//オゾン吸収係数
float3 SigmaOzone(float h)
{
    float3 coeff = float3(0.650e-6, 1.881e-6, 0.085e-6);
    float ozoneFactor = max(0.0, 1.0 - abs((h - 25000.0) / 15000.0));
    return coeff * ozoneFactor;
}

//フェーズ関数(レイリー散乱)
float RayleighPhase(float cos_theta)
{
    return (3.0f / (16.0 * PI)) * (1.0f + cos_theta * cos_theta);

}

//フェーズ関数(ミー散乱)
float MiePhase(float cos_theta,float g)
{
    //ヘンリー・グリーンスタイン関数
    float g2 = g * g;

    return (3.0f / (8.0f * PI)) * ((1.0f - g2) * (1.0f + cos_theta * cos_theta)) /
       (pow(1.0f + g2 - 2.0f * g * cos_theta, 1.5f) * (2.0f + g2));

}

//疑似ランベルト・ベールの法則（透過率）
float3 TransmittanceNumerical(float3 startPos, float3 endPos, int numSamples)
{
    float3 dir = normalize(endPos - startPos);
    float distance = length(endPos - startPos);
    float stepSize = distance / numSamples;
    float3 transmittance = float3(1.0, 1.0, 1.0);

    for (int i = 0; i < numSamples; ++i)
    {
        float t = (i + 0.5) * stepSize;
        float3 samplePos = startPos + dir * t;
        float h = length(samplePos) - 6360000.0;
        h = max(0.0, length(samplePos) - 6360000.0);
        float3 sigma_t = SigmaRayleigh(h) + SigmaMie(h) + SigmaOzone(h);
        transmittance *= exp(-sigma_t * stepSize);
    }

    return transmittance;
}


//散乱光の計算(シングルスキャッタリング＋その地点までの散乱光)
float3 ComputeSkyColor(float3 cameraPos, float3 viewDir, float3 lightDir)
{
    const int numSamples = 16;
    const float atmosphereHeight = 100000.0;
    float stepSize = atmosphereHeight / numSamples;
    float3 result = float3(0.0, 0.0, 0.0);
    
    float cos_theta = clamp(dot(viewDir, lightDir), -1.0f, 1.0f); //視線と太陽の角度
    float3 Ei = float3(1.0, 0.9, 0.7); //太陽光の色
    
    for (int i = 0; i < numSamples; ++i)
    {
        float t = i * stepSize;
        float3 samplePos = cameraPos + viewDir * t;
        float h = max(0.0f, length(samplePos) - 6360000.0);
        float3 sigma_s = SigmaRayleigh(h); // or SigmaMie(h)
        float phase = min(RayleighPhase(cos_theta),0.05f);
        
        float3 T1 = TransmittanceNumerical(cameraPos, samplePos, 8);
        float3 T2 = TransmittanceNumerical(samplePos, samplePos + lightDir * atmosphereHeight, 8);
        
        result += T1 * sigma_s * phase * T2 * Ei * stepSize;
    }

    

    
    return result;
}


//大気散乱
float4 main(VS_OUT pin):SV_TARGET
{
    float4 color = sky_texture.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
  
    //大気散乱    
    float3 sky_color = directional_light.color.xyz;

    float sun_distance = 15000000000.f;//太陽までの距離
    float3 sun_pos = normalize(-directional_light.direction.xyz) * sun_distance; //太陽の位置()
    float3 sun_dir = normalize(sun_pos.xyz-pin.world_pos.xyz);//頂点ー＞太陽
    float3 position = pin.world_pos.xyz + float3(0.f, 6360000.f, 0.f);
    
    float3 light_dir = normalize(-directional_light.direction.xyz);
    
    ////太陽から頂点までの散乱
    sky_color = ComputeSkyColor(
    position,
    sun_dir,
    light_dir);
    
    //float3 viewDir = float3(0,1,0); //camera->頂点まで方向
    float3 viewDir = normalize(pin.world_pos.xyz - camera_position.xyz); //camera->頂点まで方向
    //頂点からカメラまでの
    position = (camera_position.xyz) + float3(0.f, 6360000.f, 0.f);
    
    sky_color += ComputeSkyColor(
    position,
    viewDir,
    light_dir
    );
    
    const float sol_size = 0.00872663806;
    const float sun_disk_scale = 2.0; // [0.0, 360.0]
	// solar disk and out-scattering
    float sun_angular_diameter_cos_min = cos(sol_size * sun_disk_scale);
    float sun_angular_diameter_cos_max = cos(sol_size * sun_disk_scale * 0.5);
    
    float cos_theta = clamp(dot(viewDir, light_dir), -1.0f, 1.0f); //視線と太陽の角度
    float3 Ei = float3(1.0, 0.9, 0.7); //太陽光の色
    
    //太陽
    float sun_disk = smoothstep(sun_angular_diameter_cos_min, sun_angular_diameter_cos_max, cos_theta);
    float3 Lo = sun_disk * Ei * directional_light.intensity;
    // 太陽のディスク内に視線が入っているときだけ加算
    if (sun_disk > 0.01f) // しきい値で完全に限定
    {
        sky_color += Lo * 0.04f;
    }
    
    sky_color *=directional_light.intensity;
    
    return float4(sky_color.xyz, 1.0f);
}