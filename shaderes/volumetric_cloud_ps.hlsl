#include "fullscreen_quad.hlsli"
#include "volumetric_cloud.hlsli"
#include"scene_constant_buffer.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
SamplerState sampler_states[6] : register(s0);

Texture2D<float4> layout_cloud_height_profile : register(t0);
Texture2D<float4> layout_cloud_global_pattern : register(t1);
Texture2D<float4> layout_global_cloud_mask : register(t3);


// 2D 回転
float2 rotate2D(float2 p, float angleRad)
{
    float s = sin(angleRad);
    float c = cos(angleRad);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

// 視線と「高さ=CloudBaseAltitudeKm」の水平面の交差点を求め、XY(単位: km) を返す。
// ここでは世界 Y を「高さ」と仮定。プロジェクトの Up 軸に応じて変更。
float2 worldXYOnCloudPlaneKm(float3 roWS, float3 rdWS)
{
    float cloudBaseM = 6.4f * 1000.0;
    float t = (cloudBaseM - roWS.y) / max(rdWS.y, 1e-6);
    float3 pWS = roWS + rdWS * t;
    return float2(pWS.x, pWS.z) / 1000.0; // m→km
}

// 風による平行移動（km）
float2 windOffsetKm()
{
    float3 w = layout_window_controls.xyz * layout_window_controls.w; // 強度をまとめる
    // Z 風成分は回転に寄与させず、XY 移動として使う
    return float2(w.x, w.y) * options.z; // 単純に時間で移動
}

// レイアウト UV（タイプ別スケールあり）を作る
float2 makeLayoutUV(float2 worldXYKm, float typeScaleKm)
{
    // グローバル繰り返し距離（km）→UVへ： 1 タイル = GlobalScale km
    float2 uv = worldXYKm / max(layout_cloud_global_scale, 1e-3);

    // タイプ別の追加スケール（km）
    uv *= (layout_cloud_global_scale / max(typeScaleKm, 1e-3)); // 大きさ調整

    // グローバルオフセット＆回転
    float2 offsetKm = layout_global_texture_placement.xy + windOffsetKm();
    float rotRad = layout_global_texture_placement.z;

    uv = (worldXYKm + offsetKm) / max(layout_cloud_global_scale, 1e-3);
    uv = rotate2D(uv, rotRad);

    return uv;
}

// マスクの適用（追加/削除）
// maskContributionA: 全体寄与 (0..1)
// typeMaskStrength: タイプ別の強さ (0..1)
// 考え方: mask 値 0.5 を基準に、>0.5 で追加、<0.5 で削除
float applyMask(float pattern, float mask, float maskContributionA, float typeMaskStrength)
{
    float delta = (mask - 0.5) * 2.0; // -1..+1
    float m = maskContributionA * typeMaskStrength;
    return saturate(pattern + delta * m);
}
float2 domainWarp(Texture2D<float4> tex, float2 uv, float scale, float strength)
{
    float2 warp = tex.SampleLevel(sampler_states[LINEAR_WRAP], uv * scale, 0).rg * 2.0 - 1.0;
    return uv + warp * strength;
}



float4 main(VS_OUT pin):SV_TARGET
{
    float4 ndc = float4(2.0 * pin.texcoord.x - 1.0, 1.0 - 2.0 * pin.texcoord.y, 0.0, 1.0);
    float4 pos = mul(ndc, inverse_view_projection_transform);
    pos /= pos.w;

    float3 ray_dir = normalize(pos.xyz - camera_position.xyz);
    // 雲層の水平面との交差からワールドXY(km)を取得
    float2 worldXYKm = worldXYOnCloudPlaneKm(camera_position.xyz, ray_dir);

    // タイプ別スケール（km）
    float4 typeScaleKm = layout_cloud_per_type_scale;

    // タイプごとに UV を生成し、GlobalPattern と GlobalMask を取得して合成
    float4 coverage = 0;

    // R=層積雲
    {
        float2 uvR = makeLayoutUV(worldXYKm, typeScaleKm.r);
        float pR = layout_cloud_global_pattern.Sample(sampler_states[LINEAR_WRAP], uvR).r;
        float mR = layout_global_cloud_mask.Sample(sampler_states[LINEAR_WRAP], uvR).r;
        float pr = applyMask(pR, mR, layout_cloud_type_mask.a, layout_cloud_type_mask.r);
        float vis = layout_cloud_type.r; // 可視性
        coverage.r = saturate(pr * vis + layout_global_coverage);
    }
    // G=高層雲
    {
        float2 uvG = makeLayoutUV(worldXYKm, typeScaleKm.g);
        float pG = layout_cloud_global_pattern.Sample(sampler_states[LINEAR_WRAP], uvG).g;
        float mG = layout_global_cloud_mask.Sample(sampler_states[LINEAR_WRAP], uvG).g;
        float pg = applyMask(pG, mG, layout_cloud_type_mask.a, layout_cloud_type_mask.g);
        float vis = layout_cloud_type.g;
        coverage.g = saturate(pg * vis + layout_global_coverage);
    }
    // B=巻層雲
    {
        float2 uvB = makeLayoutUV(worldXYKm, typeScaleKm.b);
        float pB = layout_cloud_global_pattern.Sample(sampler_states[LINEAR_WRAP], uvB).b;
        float mB = layout_global_cloud_mask.Sample(sampler_states[LINEAR_WRAP], uvB).b;
        float pb = applyMask(pB, mB, layout_cloud_type_mask.a, layout_cloud_type_mask.b);
        float vis = layout_cloud_type.b;
        coverage.b = saturate(pb * vis + layout_global_coverage);
    }
    // A=乱層雲（ストーム）
    {
        float2 uvA = makeLayoutUV(worldXYKm, typeScaleKm.a);
        float pA = layout_cloud_global_pattern.Sample(sampler_states[LINEAR_WRAP], uvA).a;
        float mA = layout_global_cloud_mask.Sample(sampler_states[LINEAR_WRAP], uvA).a;
        float pa = applyMask(pA, mA, layout_cloud_type_mask.a, /*乱層はBの次＝Aなので*/1.0);
        float vis = layout_cloud_type.a;
        coverage.a = saturate(pa * vis + layout_global_coverage);
    }

    // ここでは HeightProfile はレイマーチで使うため未使用。
    // 必要なら高度の代表値（例: 雲底~雲頂の中間）を擬似的に選び、プロファイルを参照して係数を付与しても良い。

    return coverage; // RGBA=タイプ別カバレッジ
}