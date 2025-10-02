#include"geometric_primitive.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    //簡易的なディフューズライティング
    float3 light_direction = normalize(float3(0.0f,1.0f,-1.0f));
    float diffuse_intensity = max(dot(pin.normal, light_direction),0.0f);

    //最終色を計算
    float4 base_color = pin.color;
    //環境光＋拡散光
    float4 final_color = base_color * (0.3f + (0.7f * diffuse_intensity));
    final_color.a = base_color.a;
    return final_color;
}