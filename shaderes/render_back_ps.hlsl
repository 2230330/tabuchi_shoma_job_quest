#include"render_back.hlsli"

// 原点から +Z 方向を向くカメラを想定
float sphereSdf(float3 p, float3 center, float radius)
{
    return length(p - center) - radius;
}

float4 main(VS_OUT input) : SV_TARGET
{
    // SV_POSITION はピクセルシェーダーではピクセル座標
    // 画面サイズで割り、-1..+1 のスクリーン座標へ変換
    float2 screenSize = float2(1280.0f, 720.0f); // 実際の画面サイズに置き換える
    float2 screenPoint = (input.position.xy / screenSize) * 2.0f - 1.0f;

    // 横長画面でも球が円形に見えるように補正
    screenPoint.x *= screenSize.x / screenSize.y;

    // 各ピクセルからレイを作る
    float3 rayOrigin = float3(0.0f, 0.0f, 0.0f);
    float3 rayDirection = normalize(float3(screenPoint, 1.0f));

    // レイマーチ
    float t = 0.0f;
    const int maxSteps = 96;
    const float hitEpsilon = 0.001f;
    const float maxDistance = 100.0f;

    bool hit = false;

    for (int i = 0; i < maxSteps; ++i)
    {
        float3 p = rayOrigin + rayDirection * t;
        float distanceToSphere =
            sphereSdf(p, float3(0.0f, 0.0f, 3.0f), 1.0f);

        if (distanceToSphere < hitEpsilon)
        {
            hit = true;
            break;
        }

        t += distanceToSphere;

        if (t > maxDistance)
        {
            break;
        }
    }

    if (hit)
    {
        return float4(0.2f, 0.6f, 1.0f, 1.0f);
    }

    return float4(0.05f, 0.05f, 0.08f, 1.0f);
}