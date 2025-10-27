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
    float ozoneFactor = max(0.0, 1.0 - abs((h - 50000.0) / 10000.0));
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

//複数回散乱して入る入散乱の光を計算する
float3 PrecomputeMultiScattering(float3 position/*地球半径加算済み*/, float3 view_dir, float3 light_dir)
{
    float3 sample_pos = position + view_dir * 25000.f;//25Kmくらいを想定
    const int num_sample = 16;
    
    //高度計算
    float h = max(0.0f,length(sample_pos) - 6360000.f);//自身の位置-地球半径
    //高さが0以下の時、推定地面の方向なので切り上げる
    if (h < -0.01f)
    {
        return float3(0, 0, 0);
    }
    
    //散乱係数
    float3 sigma_s = SigmaRayleigh(h) + SigmaMie(h);
    float3 sigma_t = sigma_s + SigmaOzone(h);
    
    //太陽光のエネルギー
    float3 Ei = float3(1.0f, 0.9f, 0.7f);
    
    //位相関数(散乱光の内こちらに向く割合)
    float cos_theta = clamp(dot(view_dir, light_dir), -1.0f, 1.0f);
    float phase = RayleighPhase(cos_theta) + min(MiePhase(cos_theta, 0.8f), 0.0005f);
    
    //太陽方向
    float3 T1 = TransmittanceNumerical(sample_pos, (sample_pos + light_dir * 30000.f), num_sample);
    //中継点方向
    float3 T2 = TransmittanceNumerical(position, sample_pos, num_sample);
    
    //二次入散乱の近似
    float3 L2 = T1 * sigma_s * phase * Ei;
    //代表値
    float3 omega = sigma_s / max(sigma_t, float3(1e-6, 1e-6, 1e-6)); // アルベド（散乱/全係数）
    float omega_avg = (omega.x + omega.y + omega.z) / 3.0;
    float f_ms = saturate(omega_avg * 0.9);
    float F_ms = 1.0 / max(1.0 - f_ms, 1e-3f);
    F_ms = min(F_ms, 4.0f); // 最大2~4倍程度に制限（経験則）

    float3 psi_ms = L2 * F_ms * T2;
    
    return psi_ms;
}

//散乱光の計算(シングルスキャッタリング＋その地点までの散乱光)
float3 ComputeSkyColor(float3 cameraPos, float3 viewDir, float3 lightDir)
{
    const int numSamples = 4;
    const float atmosphereHeight = 100000.0;
    float stepSize = atmosphereHeight / numSamples;
    float3 result = float3(0.0, 0.0, 0.0);
    
    float cos_theta = clamp(dot(viewDir, lightDir), -1.0f, 1.0f); //視線と太陽の角度
    float3 Ei = float3(1.0, 0.9, 0.7); //太陽光の色

    for (int i = 0; i < numSamples; ++i)
    {
        float t = i * stepSize;
        float3 samplePos = cameraPos + viewDir * t;
        float h = max(0.0f, length(samplePos) - 6360000.0f);
        if(h<-0.1f)
        {
            continue;
        }
        
        float3 sigma_t = SigmaRayleigh(h) + SigmaMie(h) + SigmaOzone(h);
        
        //太陽に近いほど1.0
        float angle_factor = saturate(cos_theta);
        float phase_rayliegh =min(RayleighPhase(cos_theta),0.05f) * angle_factor;
        float phase_mie = min(MiePhase(cos_theta, 0.8f), 0.0005f);
        
        float3 T1 = TransmittanceNumerical(cameraPos, samplePos, numSamples);
        float3 T2 = TransmittanceNumerical(samplePos, samplePos + lightDir * atmosphereHeight, numSamples);
        
        result += T1 * sigma_t * ((phase_rayliegh + phase_mie)) * T2 * Ei * stepSize;
    }
    
    return result;
}

//大気散乱
float4 main(VS_OUT pin):SV_TARGET
{
    float4 color = sky_texture.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
  
    //大気散乱    
    float3 sky_color = float3(0, 0, 0);

    float sun_distance = 150000000000.f;//太陽までの距離1億5千万キロメートルをメートル表記で
    float3 light_dir = normalize(-directional_light.direction.xyz);
    float3 sun_pos = light_dir * sun_distance; //太陽の位置()
    float3 sun_dir = normalize(sun_pos.xyz-pin.world_pos.xyz);//頂点ー＞太陽
    //カメラの現在位置を地球の半径分押し上げる
    float3 position = camera_position.xyz + float3(0.f, 6360000.f, 0.f);
    //カメラから天球の各頂点への方向
    float3 viewDir = normalize(pin.world_pos.xyz - camera_position.xyz); //camera->頂点まで方向
    
    //疑似多重散乱の事前計算
    float3 multi_scattaring = PrecomputeMultiScattering(position, viewDir, light_dir);
    
    ////シングルスキャッタリング
    float3 single_scattaring = ComputeSkyColor(
    position,
    viewDir,
    sun_dir);
    
    sky_color = (multi_scattaring /*+ single_scattaring*/);
    
    //太陽
    {
        const float sol_size = 0.00872663806;
        const float sun_disk_scale = 2.0; // [0.0, 360.0]
	    // solar disk and out-scattering
        float sun_angular_diameter_cos_min = cos(sol_size * sun_disk_scale);
        float sun_angular_diameter_cos_max = cos(sol_size * sun_disk_scale * 0.5);
        
        float cos_theta = clamp(dot(viewDir, light_dir), -1.0f, 1.0f); //視線と太陽の角度
        float3 Ei = float3(1.0, 0.9, 0.7); //太陽光の色
        
        float sun_disk = smoothstep(sun_angular_diameter_cos_min, sun_angular_diameter_cos_max, cos_theta);
        float3 Lo = sun_disk * Ei * directional_light.intensity;
        // 太陽のディスク内に視線が入っているときだけ加算
        if (sun_disk > 0.01f) // しきい値で完全に限定
        {
            sky_color += Lo * 0.04f;
        }
    }
    
    sky_color *= directional_light.intensity;
    
    return float4(sky_color.xyz, 1.0f);
}