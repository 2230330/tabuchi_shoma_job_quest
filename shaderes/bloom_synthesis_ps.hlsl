
#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
SamplerState sampler_states[5] : register(s0);

static const uint synthesis_count = 2;
Texture2D downsampled_textures[synthesis_count] : register(t0);

//ポストエフェクトの結果を合成するためのシェーダー
float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    float4 sampled_color = 0;
    float total_alpha = 0;
    float total_weight = 0;

    // ダウンサンプルごとに強度を減らす
    [unroll]
    for (uint i = 0; i < synthesis_count; ++i)
    {
        float weight = 1.0 / pow(2.0, i); // 各レベルの寄与を減らす
        sampled_color += downsampled_textures[i].Sample(sampler_states[LINEAR_CLAMP], texcoord) * weight;
        total_alpha += sampled_color.a; // アルファも加算
        total_weight += weight;
    }

    //sampled_color /= total_weight; // 平均化
    
    if(total_alpha>1.0f)
    {
        total_alpha = 1.0f; // アルファ値をクランプ
    }
    
    return float4(sampled_color.rgb,total_alpha);

}
