// ライトシャフト用：R値のみの雲存在マップ
#include "fullscreen_quad.hlsli"
#include "camera_buffer.hlsli"
#include "forward_light.hlsli"

#define LINEAR_CLAMP 3
SamplerState sampler_states[6] : register(s0);

Texture2D cloud_texture : register(t0);

// 太陽のスクリーンUV取得
float2 GetSunScreenUV()
{
    float3 sunDir = normalize(-directional_light.direction.xyz);

    // 十分遠方の仮想太陽位置
    float3 sunWorldPos = camera_position.xyz + sunDir * 100000.0;

    float4 sunClip = mul(
        float4(sunWorldPos, 1.0),
        view_projection_transform
    );

    float2 sunUV = sunClip.xy / sunClip.w;
    sunUV = sunUV * 0.5 + 0.5;

    return sunUV;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

    // 太陽のスクリーン位置
    float2 sunUV = GetSunScreenUV();
    
    if(dot(uv,sunUV)<0.0)
    {
        // 太陽が画面外ならシャフト無し
        clip(-1);
    }

    // 放射方向（UV空間）
    float2 dir = sunUV - uv;
    float dist = length(dir);
    dir /= max(dist, 1e-4);

    const int SAMPLE_COUNT = 32;
    const float BLUR_LENGTH = 0.03;

    float sum = 0.0;
    float weightSum = 0.0;

    [loop]
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        float t = (i + 1.0) / SAMPLE_COUNT;

        float2 sampleUV = uv + dir * t * BLUR_LENGTH;

        float cloud = cloud_texture.SampleLevel(
            sampler_states[LINEAR_CLAMP],
            sampleUV,
            0
        ).r;

        // 太陽側を強く
        float w = 1.0 - t;

        sum += cloud * w;
        weightSum += w;
    }

    float blurredCloud = sum / max(weightSum, 1e-5);

    // シャフト強度（雲があるほど減衰）
    float shaft = saturate(blurredCloud * 1.5);

    return float4(shaft, shaft, shaft, 1);
}
