#include"render_back.hlsli"

float4 main(VS_OUT input) : SV_TARGET
{
    float t = saturate(input.uv.y);
    float3 topColor = float3(0.05, 0.15, 0.35);
    float3 bottomColor = float3(0.75, 0.45, 0.25);

    return float4(lerp(topColor, bottomColor, t), 1.0);
}