#include "../swirly_curl_noise.hlsli"

RWTexture2D<float4> cloud_global_mask : register(u0);

#define TAU 6.28318530718
#define GLOBAL_CLOUD_MASK_DIMENSIONS 256
#define GLOBAL_CLOUD_MASK_NUMTHREADS 8
#define MAX_ITER 8

// Shadertoy ベースの水面ハイライト
float waterHighlight(float2 p, float time, float foaminess)
{
    float2 i = p;
    float c = 0.0;

    float foaminess_factor = lerp(1.0, 3.0, foaminess);
    float inten = 0.005 * foaminess_factor;

    [loop]
    for (int n = 0; n < MAX_ITER; n++)
    {
        float t = time * (1.0 - (3.5 / float(n + 1)));

        i = p + float2(
            cos(t - i.x) + sin(t + i.y),
            sin(t - i.y) + cos(t + i.x)
        );

        c += 1.0 / length(float2(
            p.x / sin(i.x + t),
            p.y / cos(i.y + t)
        ));
    }

    c = 0.2 + c / (inten * MAX_ITER);
    c = 1.17 - pow(c, 1.4);
    c = pow(abs(c), 8.0);

    return c / sqrt(foaminess_factor);
}

[numthreads(GLOBAL_CLOUD_MASK_NUMTHREADS, GLOBAL_CLOUD_MASK_NUMTHREADS, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= GLOBAL_CLOUD_MASK_DIMENSIONS ||
        dtid.y >= GLOBAL_CLOUD_MASK_DIMENSIONS)
        return;

    float2 uv = (float2) (dtid) / GLOBAL_CLOUD_MASK_DIMENSIONS;
    float3 uvw = float3(uv, 0.0);
    float dist_center = pow(2.0 * length(uv - 0.5), 2.0);
    float foaminess = smoothstep(0.4, 1.2, dist_center);
    float time = (1.0f / 60.0f) * 0.1 + 23.0;

    float2 p_base = uv * TAU - 250.0;
    

    //==================================================
    // R
    float r;
    {
        float base = waterHighlight(p_base, time, foaminess);
        base = saturate(base);
        base = 1.2f - base; // 反転＋少し明るめ
        // 軽くガンマ
        base = pow(base, 1.0f); // 必要なら 0.9~1.2 で微調整
        r = base;
    }

    //==================================================
    // G:
    float g;
    {
        float gh = waterHighlight(p_base, time, foaminess);
        gh = saturate(gh);
        gh = 1.2f - gh; // 反転

        float3 flow_g = FlowField(uvw * 10.0f, 1.0f, 8.0f);
        float flow_dir = abs(dot(normalize(flow_g), float3(0.0, 1.0, 0.0)));
        flow_dir = pow(flow_dir, 2.0f); // 線っぽさ強調

        // ハイライト：80%、方向性: 20% 混合
        g = lerp(gh, flow_dir, 0.2f);
        g = saturate(g);
        // G は R より少しコントラスト低め
        g = pow(g, 1.1f);
    }

    //==================================================
    // B
    float b;
    {
        float2 p_b = (uv * 1.2f - 0.1f) * TAU - 250.0; // ほんの少しスケール＆オフセット

        float bh = waterHighlight(p_b, time, foaminess);
        bh = saturate(bh);
        bh = 1.2f - bh;

        // 少しだけ FlowField で乱しを入れる
        float3 flow_b = FlowField(uvw * 4.0f, 1.5f, 32.0f);
        float flow_mag = length(flow_b);
        flow_mag = pow(flow_mag, 1.0f);

        b = lerp(bh, flow_mag, 0.15f); // 15% だけ揺らぎを足す
        b = saturate(b);
        b = pow(b, 0.9f); // コントラスト強め(暗いところを締める)
    }
    // A は今回は使わない
    float a = 0.0f;

    cloud_global_mask[dtid] = float4(r, g, b, a);
}