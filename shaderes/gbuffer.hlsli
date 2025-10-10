#ifndef _GBUFFER_HLSLI_
#define _GBUFFER_HLSLI_

//シェーディング方式を決める為のID
static const int shading_model_unlit = 0; //ライティング無し
static const int shading_model_phong_shading = 1; //ふぉんしぇーでぃんぐ

static const int shading_model_max = 2; //最大数

//ピクセルシェーダーへの出力用構造体
struct PSGBufferOut
{
    float4 base_color : SV_TARGET0;
    float4 emissive_color : SV_TARGET1;
    float4 normal_depth : SV_TARGET2;
};

//GBUffer情報構造体
struct GBufferData
{
    float3 base_color; //ベースカラー
    float3 emissive_color; //自己発光色
    float3 w_normal; //ワールド法線
    float3 w_position; //ワールド座標
    float depth; //深度(Decode時のみ)
    int shading_model; //シェーディング方法を決めるID
};
//GBufferDataに纏めた情報をピクセルシェーダーの出力構造体に変換
PSGBufferOut EncodeGBuffer(in GBufferData data, matrix view_projection__matrix)
{
    PSGBufferOut ret = (PSGBufferOut) 0;
    //ベースカラーの設定
    ret.base_color.rgb = data.base_color.rgb;
    ret.base_color.a = 1.0f;
    //自己発光色の設定
    ret.emissive_color.rgb = data.emissive_color;
    ret.emissive_color.a = 1.0f;
    //法線情報の説明
    ret.normal_depth.rgb = data.w_normal;
    float4 position = mul(float4(data.w_position, 1.0f), view_projection__matrix);
    ret.normal_depth.a = position.z / position.w;
    
    ret.base_color.a = ((float) data.shading_model) / shading_model_max;
    
    return ret;
}
//Gbufferテクスチャ受け流しよう構造体
struct PSGbufferTextures
{
    SamplerState state;
    Texture2D<float4> base_color;
    Texture2D<float4> emissive_color;
    Texture2D<float4> normal_depth;
};
//ピクセルシェーダーの出力用構造体からGbuffer情報に変換
GBufferData DecodeGBuffer(PSGbufferTextures textures, float2 uv, matrix inverse_view_projection_transform)
{
    //各テクスチャから情報を取得
    float4 base_color = textures.base_color.SampleLevel(textures.state, uv, 0);
    float4 emissive_color = textures.emissive_color.SampleLevel(textures.state, uv, 0);
    float4 normal_depth = textures.normal_depth.SampleLevel(textures.state, uv, 0);
    
    GBufferData ret = (GBufferData) 0;
    ret.base_color = base_color.rgb;
    ret.emissive_color = emissive_color.rgb;
    ret.w_normal = normalize(normal_depth.rgb);
    ret.depth = normal_depth.a;
    
    float4 position = float4(uv.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), ret.depth, 1.0f);
    position = mul(position, inverse_view_projection_transform);
    ret.w_position = position.xyz / position.w;
    
    ret.shading_model = (int) (base_color.a * shading_model_max + 0.5f);
    
    return ret;
}
#endif //_GBUFFER_HLSLI_