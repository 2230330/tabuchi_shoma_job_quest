#include "fullscreen_quad.hlsli"
#include "camera_buffer.hlsli"
#include "scene_constant_buffer.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
SamplerState sampler_states[6] : register(s0);

Texture2D<float4> normal_texture : register(t0);
Texture2D<float> depth_texture : register(t1);
Texture2D<float4> color_texture : register(t2);
Texture2D<float4> parameter_texture : register(t3);

cbuffer SsrConstants : register(b9)
{
    float distance;
    int num_steps;
    int max_mip;
    float thickness;
    
    float resolution;
    float start_bias;
    float intensity;
}

float3 GetNormalVS(float2 uv)
{
    //テクスチャはワールド空間で算出されている 
    //gltf_model_gbuffer_vs.hlsl
    float3 n= normal_texture.SampleLevel(sampler_states[POINT_CLAMP], uv, 0).xyz;
    
    //ワールド空間からからビュー空間へ変換
    n = normalize(mul(float4(n, 0), view_transform));
    
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    
    
    return normalize(n);
}
//線形深度を入れる
float GetSceneDepth(float2 uv, int mip)
{
    return depth_texture.SampleLevel(
        sampler_states[POINT_CLAMP], uv, mip);
}
//色情報
float3 GetColor(float2 uv)
{
    return color_texture.Sample(
    sampler_states[POINT_CLAMP], uv
    ).xyz;

}
//1:メタリック、2:ラフネス
float2 GetParameter(float2 uv)
{
    return parameter_texture.Sample(
        sampler_states[POINT_CLAMP], uv
    ).xy;
}

//screen -> view
float3 ReconstructViewPosition(float2 uv, float linear_z,float2 projection_scale)
{
    
    //screen->ndc
    float2 ndc = uv * 2.0 - 1.0;
    //DirectXスクリーンスペースコンバージョン
    ndc.y *= -1.0f;
    //リニア深度
    float view_z = max(linear_z, 1e-6);
    //リコンストラクション
    float2 view_xy = ndc * view_z / projection_scale;
    
    return float3(view_xy, view_z);
}

float2 ViewToScreen(float3 view_pos,uint2 dimensions)
{
    // View → Clip
    float4 clip = mul(float4(view_pos, 1.0f), projection_transform);
    // Clip → NDC
    clip.xyz /= clip.w;

    // NDC → UV
    float2 uv;
    uv = clip * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;

    return uv*dimensions;
}

float perspectiveCorrectZ(float z0, float z1, float t)
{
    return (z0 * z1) / lerp(z1, z0, t);
}

//反射ベクトルが外に出ていないかを確認
bool OutOfBounds(float2 uv)
{
    return uv.x <= 0 || uv.x >= 1 || uv.y <= 0 || uv.y >= 1;
}

//hi-zでのループ時の制御用
int ComputeMip(float2 delta,float2 dims)
{
    float2 texel_size = abs(delta) / dims;
    float max_component = max(texel_size.x, texel_size.y);
    
    float mip = log2(max_component * max(dims.x, dims.y));
    return clamp((int) mip, 0, max_mip);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    
    //オブジェクトのパラメータ
    //1:メタリック、2:ラフネス
    //float2 parameter = GetParameter(pin.texcoord);
    //if(parameter.y>0.8f)
    //    return float4(0, 0, 0, 0);
    
    //View空間のリニア深度
    float scene_depth = GetSceneDepth(pin.texcoord.xy, 0);
    if (scene_depth <= 0 || scene_depth >= 1.0f)
    {
        return float4(0, 0, 0, 0);
    }
    scene_depth *= camera_clip_distance.y;
    //View空間のレイを取得
    float2 projection_scale = float2(projection_transform._11, projection_transform._22);
    float3 view_pos = ReconstructViewPosition(pin.texcoord.xy, scene_depth, projection_scale);    
    float3 n = normalize(GetNormalVS(pin.texcoord));
    float3 v = normalize(view_pos);
    float3 r = normalize(reflect(v, n));
    
    //ある程度反射方向と視線方向の角度が近い場合、早期処理を行う。
    //SSRはスクリーン空間で行うため、画面に映っていない裏側を映すことが出来ない為、
    //こちら側へと伸びて来るレイには対応できない
    if(dot(-v,r)>0.0f)
    {
        return float4(0, 0, 0, 0);
    }
    
    float3 view_end = view_pos + (r * distance);
    
    //スクリーンスペースに
    uint2 dimensions;
    uint mip = 0, levels;
    normal_texture.GetDimensions(mip, dimensions.x, dimensions.y, levels);
    float2 start_frag = ViewToScreen(view_pos + (r*start_bias), dimensions);
    float2 end_frag = ViewToScreen(view_end, dimensions);
    
    float2 delta = end_frag - start_frag;
    
    bool use_x = abs(delta.x) > abs(delta.y);
    
    float max_delta = use_x ? abs(delta.x) : abs(delta.y);
    float steps = max_delta * saturate(resolution);
    
    if (steps < 1.0f)
        return (float4) 0;
    
    //最大ステップ数を制限
    if (steps > 2000)
        steps = 2000;
    
    
    float2 increment = delta / steps;
    float2 frag = start_frag;
    float2 uv;
    
    float t_min = 0.0f;
    float t_max = 0.0f;
    
    bool hit = false;
    float depth_delta = 0.0f;
    
    float t = 0.0f;

    
    //coarse march
    [loop]
    for (int i = 0; i < (int) steps; ++i)
    {
    
        frag += increment;
        
        uv = frag / dimensions;
        
        if (OutOfBounds(uv))
        {
            return (float4)0;
        }
        
        float scene_linear_depth = GetSceneDepth(uv, 0);
        scene_linear_depth *= camera_clip_distance.y;
        float3 scene_pos = ReconstructViewPosition(uv, scene_linear_depth, projection_scale);
        
        //compute interpolation parameter along ray
        if (use_x)
            t = (frag.x - start_frag.x) / delta.x;
        else
            t = (frag.y - start_frag.y) / delta.y;
        
        t = saturate(t);
        
        float view_z = perspectiveCorrectZ(view_pos.z, view_end.z, t);
        
        depth_delta = view_z - scene_pos.z;
        
        if (depth_delta > 0 && depth_delta < thickness)
        {

            {
                hit = true;
                t_max = t;
                break;
            }
        }
        
        t_min = t;
    }
    
    //もし当たっていなかったらここで終了
        if (!hit)
        return (float4) 0;
    
    //Binary refinement
    t = 0.5f * (t_min + t_max);
    
    [loop]
    for (int i = 0; i < num_steps;i++)
    {
        float2 frag_refined = lerp(start_frag, end_frag, t);
        float2 uv = frag_refined / dimensions;
        
        if(OutOfBounds(uv))
            return (float4) 0;
        
        float scene_linear_depth = GetSceneDepth(uv, mip);
        scene_linear_depth *= camera_clip_distance.y;
        float3 scene_pos = ReconstructViewPosition(uv, scene_linear_depth, projection_scale);
        
        float view_z = perspectiveCorrectZ(view_pos.z, view_end.z, t);
        depth_delta = view_z - scene_pos.z;

        if (depth_delta > 0 && depth_delta < thickness)
            t_max = t;
        else
            t_min = t;

        t = 0.5 * (t_min + t_max);
    }
    
    //final hit information
    float2 hit_uv = lerp(start_frag, end_frag, t) / dimensions;
    if(OutOfBounds(hit_uv))
    {
        return (float4) 0;
    }
    float hit_depth = GetSceneDepth(hit_uv, mip) * camera_clip_distance.y;
    float3 hit_pos = ReconstructViewPosition(hit_uv, hit_depth, projection_scale);
    
    //visibility weighting
    float visibility = 1.0f;
    
    //fade reflections facing toward the camera 
    visibility *= (1.0f - max(dot(-v, r), 0));
    //fade based on depth mismatch within thickness tolerance
    visibility *= (1.0f - saturate(depth_delta / thickness));
    //fade near maximum ray distance to avoid hard cutoff
    visibility *= (1.0f - saturate(length(hit_pos - view_pos) / distance));
    
    //画面端に近づくほど透過させる
    const float ssr_border = 0.3f;
    visibility *= smoothstep(1.0f, 1.0f - ssr_border, saturate(2.0f * abs(hit_uv.x - 0.5f)));
    visibility *= smoothstep(1.0f, 1.0f - ssr_border, saturate(2.0f * abs(hit_uv.y - 0.5f)));
    
    
    float n_o_v = saturate(dot(n, -v));
    float f0 = 0.04f;
    float fresnel = f0 + (1.0f - f0) * pow(1 - n_o_v, 5);
    visibility *= fresnel;
    
    visibility = saturate(visibility);
    if (visibility < 0.05f)
        visibility = 0.f;
    
    return float4(GetColor(hit_uv), saturate(visibility*intensity));
    
}