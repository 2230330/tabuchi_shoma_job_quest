// godray_radial_cloud_only_ps.hlsl
#include "fullscreen_quad.hlsli"
#include "scene_constant_buffer.hlsli"
#include"volumetric_cloud.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
SamplerState sampler_states[6] : register(s0);

Texture2D<float4> cloud_map : register(t0); // R=presence, G=cloudDepth01

// ちょい高品質なノイズ（ディザ用）簡易関数
float Hash12(float2 p)
{
    // UV と time で変化する簡易ハッシュ
    float h = dot(p, float2(127.1, 311.7));
    return frac(sin(h) * 43758.5453123);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    
    float2 uv0 = pin.texcoord;

    // 太陽が無効なら何も出さない
    //if (sun_visible <= 0.001f)
    //    return float4(0, 0, 0, 0);

    float2 lightPos = sun_uv;

    // 太陽が画面外にあるときは減衰（任意）
    // 画面外でも出したいならこのブロックは削除OK
    float2 lp = lightPos;
    float inScreen =
        (lp.x >= 0.0f && lp.x <= 1.0f && lp.y >= 0.0f && lp.y <= 1.0f) ? 1.0f : 0.0f;
    if (inScreen < 0.5f)
        return float4(0, 0, 0, 1);

    // -------------------------
    // パラメータ（調整用）
    // -------------------------
    const int NUM_SAMPLES = 56; // 32～80くらいで調整
    float density = 0.95f; // 放射方向の長さ
    float decay = 0.965f; // 減衰（遠方ほど小さく）
    float weight = 0.35f; // サンプル寄与
    float exposure = 1.25f; // 最終露出

    // 雲オンリー用の調整：
    // 雲深度(G)を使って「近い雲ほど遮蔽が強い」などを作れる
    // 0: 使わない / 1: 使う
    const bool USE_CLOUD_DEPTH_WEIGHT = true;

    // 雲深度を使う場合のカーブ
    // depth01 が小さい＝近い雲 → 強く遮蔽
    float depthPower = 1.5f; // 1～3くらいで調整

    // ディザ強度（縞防止）
    float jitterStrength = 1.0f;

    // -------------------------
    // ラジアルステップ
    // -------------------------
    float2 dir = (lightPos - uv0);
    float2 stepUV = dir * (density / NUM_SAMPLES);

    // ディザ（開始位置をランダム化）
    float jitter = Hash12((uv0 * viewport_size.xy) + options.z) - 0.5f;
    float2 uv = uv0 + stepUV * jitter * jitterStrength;

    // -------------------------
    // 積分
    // -------------------------
    float illum = 0.0f;
    float illumDecay = 1.0f;

    // ループの最適化（分岐少なめ）
    [unroll]
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
        uv += stepUV;

        // 画面外なら終了
        if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
            break;

        float4 cm = cloud_map.SampleLevel(sampler_states[LINEAR_CLAMP], uv, 0);
        float cloudOcc = saturate(cm.r); // 0..1（大きいほど遮蔽）
        float cloudD = saturate(cm.g); // 0..1（雲無しなら1）

        // “光が通る量”として積分したいので透過に変換
        float trans = 1.0f - cloudOcc;

        // 近い雲ほど強く遮る（＝transをより下げる）などの調整
        // cloudDが小さいほど近い
        if (USE_CLOUD_DEPTH_WEIGHT)
        {
            float nearFactor = pow(saturate(1.0f - cloudD), depthPower); // 近いほど大きい
            // 近い雲は遮蔽強く → 透過をさらに下げる
            trans = saturate(trans * (1.0f - 0.75f * nearFactor));
        }

        illum += trans * illumDecay * weight;
        illumDecay *= decay;
    }

    // 仕上げ
    float shafts = illum * exposure;

    // 太陽中心から離れるほど弱める（自然な感じに）
    float distToLight = length(uv0 - lightPos);
    shafts *= saturate(1.0f - distToLight);

    //shafts *= sun_visible;

    return float4(shafts, shafts, shafts, 1);
}
