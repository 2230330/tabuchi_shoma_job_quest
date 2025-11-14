#include"cloud_dome.hlsli"
#include"forward_light.hlsli"
#include"scene_constant_buffer.hlsli"
#include"shading_functions.hlsli"

Texture2D noise_texture : register(t0);



//簡易雲っぽいノイズ

float SampleCloudNoise(float3 p, float scale)
{
    // ワールド空間で雲を動かす
    p.xz += wind_direction.xz * noise_seed;

    // 低周波ノイズで雲の塊を形成
    float low_freq = PerlinNoiseFBM(p * 0.001f); // 大きな雲の分布
    float high_freq = PerlinNoiseFBM(p * scale * 0.01f); // 細かいディテール
    //float n = low_freq * 0.7 + high_freq * 0.3;
    float n = low_freq * lerp(0.8f,1.2f,high_freq);
    n = pow(saturate(n), 1.5f);
    //ノイズコントラスト
    n *= noise_intensity;

    // 高度による減衰
    float height_factor = saturate((p.y - cloud_base) / (cloud_top - cloud_base));
    height_factor = smoothstep(0.2f, 0.8f, height_factor);
    n *= height_factor;

    //return saturate(n - noise_threshold);
    return n;
}
float SampleCloudDensity(float3 pos)
{
    // 雲の高さ範囲（例：コンスタントバッファの cloud_base / cloud_top を使用）
    if (pos.y < cloud_base || pos.y > cloud_top)
        return 0.0f;

    // ノイズ空間へ座標をスケール
    float3 noise_pos = pos * 0.001f + wind_direction.xyz * noise_seed;

    // 3Dノイズをサンプリング（Perlin, Worley, Curl など）
    float base_noise = SampleCloudNoise(pos,1.f);

    // 閾値で制御
    float density = saturate((base_noise - noise_threshold) * noise_intensity);

    // 高さ減衰（上空ほど薄く、下層ほど濃く）
    float height_factor = saturate((pos.y - cloud_base) / (cloud_top - cloud_base));
    density *= (1.0f - height_factor);

    return density;
}

//福井先生の奴を参考にしてみた雲生成コード
float RayMarchCloud(float3 ray_origin, float3 ray_dir, float max_distance, float base_step, int iteration)
{
    float total_density = 0.0f;
    float distance = 0.0f;
    
    const float min_step = base_step * 0.25f; //密な個所
    const float max_step = base_step * 2.0f; //ほぼ空気
    const float density_threshold = 0.1f; //密度の閾値
    
    [loop]
    for (int i = 0; i < iteration && distance < max_distance; i++)
    {
        float3 pos = ray_origin + ray_dir * distance;
        float density = SampleCloudDensity(pos);
        
        //適応ステップ制御
        float adapt = saturate(density / density_threshold);
        float adaptive_step = lerp(max_step, min_step, adapt);
        
        total_density += density * adaptive_step;
        
        //光散乱などの追加処理はここで行う
        
        distance += adaptive_step;

    }
    
    return total_density;
}

//フェーズ関数(ミー散乱)
float MiePhase(float cos_theta, float g)
{
    //ヘニエイ・グリーンスタイン関数
    float g2 = g * g;
    
    return (1.0f - g2) / (pow(1.0f + g2 - 2.0f * g * cos_theta, 1.50f) * 4.0f * PI);
}


float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = 0;
    
    float2 ndc = (pin.position.xy / viewport_size.xy) * 2.0f - 1.0f;
    ndc.y = -ndc.y;

    float4 end_pos = mul(float4(ndc, 1.0f, 1.0f), inverse_view_projection_transform);
    end_pos /= end_pos.w;

    float3 ray_start = camera_position.xyz;
    float3 ray_position = ray_start;
    float3 ray_dir = normalize(end_pos.xyz - ray_start);
    
    
    float3 sun_dir = normalize(-directional_light.direction.xyz);
    float cosTheta = dot(ray_dir, sun_dir);
    float phase = MiePhase(cosTheta, 0.1f); // g=0.8で前方散乱強め
    
    float accumulate_alpha = 0.0f;
    float shaft = 0.f;
    //レイの方向が最底辺より下なら、描画しない(天球用)
    if (dot(ray_dir, float3(0, 1, 0)) < 0.01f)
    {
        return color;
    }


    //アダプティブステップ版
    float min_step = step_size * 0.01f;
    float max_step = step_size * 2.0f;
    float distance = 0.0f;
    
    //ランベルトベール減衰
    float transmittance = 1.0f;
    float step = (cloud_top - cloud_base) / iteration;
    [loop]
    for (int i = 0; i < iteration&&distance<max_distance; i++)
    {
        ray_position =ray_start+ ray_dir * distance; // 距離関数に基づいて進む
        float density = SampleCloudDensity(ray_position);
        
        if (density > 0.001f) // ヒット
        {
            //// 散乱光（太陽方向からの寄与）
            float light_density = SampleCloudDensity(ray_position + normalize(-directional_light.direction.xyz) * 80.0f);
            float light_intensity = exp(-light_density * 2.0f); // 雲の厚みによる減衰
            float scatter = light_intensity * light_scatter_strength * intensity;

            
            // 光の吸収
            float absorption = exp(-density * step_size * 0.02f);
            transmittance *= absorption;

            // 散乱光の寄与を積分
            color.rgb += transmittance * scatter * density * phase;

            accumulate_alpha += (1.0f - absorption);
            if (accumulate_alpha >= 1.f||transmittance < 0.01f)
                break; //早期終了
        }

        //shaft += density * intensity;

        // アダプティブステップ
        float adapt = saturate(density / 0.1f);
        float adaptive_step = lerp(max_step, min_step, adapt);
        distance += adaptive_step;
    }
    
    
    
    ////平均化
    //shaft /= iteration;
    
    
    //光の散乱強度で補正
    float3 base_color = color.rgb*base_brightness;
    
    
    //shaftの影響を高める
    float3 final_color = lerp(base_color, float3(1, 1, 1), saturate(shaft * light_scatter_strength));//0.8は調整値
    
    
    //出力の透明度をalpha_scaleで制御
    return float4(final_color, accumulate_alpha * alpha_scale);
}