#include"bloom.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
SamplerState sampler_states[5] : register(s0);
//この輝度抽出は、HDR対応をするために作りました。

Texture2D hdr_color_buffer_texture : register(t0);    
float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    // 入力HDRカラー
    float3 hdrColor = hdr_color_buffer_texture.Sample(sampler_states[LINEAR_CLAMP], texcoord).rgb;

    // 最大輝度成分を使用（高速で自然）
    float brightness = max(hdrColor, float3(0.2126f,0.7152f,0.0722f));

    // ソフトニー（soft-knee）による滑らかなしきい値遷移
    float knee = bloom_extraction_threshold * bloom_soft_knee + 1e-4;
    float soft = smoothstep(bloom_extraction_threshold - knee,
                            bloom_extraction_threshold + knee,
                            brightness);

    // 高輝度抽出
    float bloomMask = saturate((brightness - bloom_extraction_threshold) / knee);
    bloomMask = max(soft, bloomMask);

    // 強度を適用して返す
    float3 result = hdrColor * bloomMask * bloom_intensity;

    return float4(result, 1.0);
}
