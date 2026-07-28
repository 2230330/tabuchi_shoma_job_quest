#include "fullscreen_quad.hlsli"
#include"fog.hlsli"
#include"deferred_rendering.hlsli"
#include"camera_buffer.hlsli"
#include"light_view_projection.hlsli"
#include"noise_functions.hlsli"
#include "scene_constant_buffer.hlsli"

//簡易的なレイマーチングフォグ



#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
#define SHADOWMAP 6
SamplerState sampler_states[7] : register(s0);

Texture2D<float> depth_map : register(t0);
Texture3D<float4> noise_map : register(t1);
static const int shadow_num = 3;
Texture2D shadow_map[shadow_num] : register(t10);

//static const float PI = 3.14159265359f;
static const float time_offset = .10f;

float SampleObjectDepth(float2 sample_point)
{
    //物体深度テクスチャから深度をサンプリングする関数
    //物体を描画する際に、雲が物体の前にあるか後ろにあるかを判断するために使用される
    //そのままサンプリングした場合、テクスチャの解像度の違いによりジャギーが発生する可能性がある為、
    //周囲のサンプルを取って最大値を返すことで、実際の物体よりも一回り小さい輪郭を作るようにしている
    float2 object_resolution = float2(object_resolution_width, object_resolution_height);
    float d = 0.0f;
    float2 texel = 1.0f / object_resolution;
    [loop]
    for (int x = -1; x <= 1; x++)
    {
        [loop]
        for (int y = -1; y <= 1; y++)
        {
            float2 offset = float2(x, y) * texel;
            float sample = depth_map.Sample(sampler_states[POINT_CLAMP], sample_point + offset);
            
            //周囲のサンプルの最大値を取ることで、物体の輪郭を少し削る
            d = max(d, sample);
            if(d>=1.0f)
            {
                break;
            }
        }
        
        if(d>=1.0f)
            break;
    }

    return d;
}

float MiePhase(float cos_theta, float g)
{
    float g2 = g * g;
    return (1.0f - g2) /
        (pow(1.0f + g2 - 2.0f * g * cos_theta, 1.5f) * 4.0f * PI);
}
float SampleFogNoiseAdvanced(float3 world_pos)
{
    
    float3 p = world_pos*noise_scale +   options.z * time_offset;

    float4 n = noise_map.SampleLevel(
        sampler_states[LINEAR_WRAP],
        p,
        0.0
    );

    float perlin_worley = n.r;
    
    float fog_shape = perlin_worley * perlin_worley;

    return saturate(fog_shape);
}
float SampleFogDensity(float3 world_pos)
{
    float noise = SampleFogNoiseAdvanced(world_pos);
    
    float height_factor = saturate(1.0f - world_pos.y / fog_max_height);
    height_factor *= height_factor;
    
    return fog_density * noise*height_factor;
}


float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = ambient_color;
    
    float4 ndc = float4(2.0 * pin.texcoord.x - 1.0, 1.0 - 2.0 * pin.texcoord.y, 0.0, 1.0);
    float4 pos = mul(ndc, inverse_view_projection_transform);
    pos /= pos.w;
    float3 ray_dir = normalize(pos.xyz - camera_position.xyz);
    
    float object_depth = SampleObjectDepth(pin.texcoord.xy);
        
        
    //距離内なら処理をしなくてもよい
    if (fog_max_distance <= 0.f)
        clip(0);
    float step_length = fog_max_distance / fog_steps;
    
    float3 ray_start = camera_position.xyz;
    float3 ray_end = camera_position.xyz + (ray_dir * fog_max_distance);
    float3 obj_pos = camera_position.xyz + (ray_dir * fog_max_distance);
    if (object_depth < 1.0f)
    {
        
        obj_pos = camera_position.xyz + (ray_dir * (camera_clip_distance.y * object_depth));
        
        //オブジェクト深度の方がレイの終わりより近いならば、
        //レイの終点をオブジェクト深度に合わせる
        //if (length(ray_end - ray_start) > length(obj_pos-ray_start))
        //    ray_end = obj_pos;

    }
    float obj_dis = length(obj_pos - ray_start);
    float3 ray_step =ray_dir*step_length;
    
    const float4x4 dither_pattern =
    {
        { 0.0f, 0.5f, 0.125f, 0.625f },
        { 0.75f, 0.25f, 0.875f, 0.375f },
        { 0.1875f, 0.6875f, 0.0625f, 0.5625f },
        { 0.9375f, 0.4375f, 0.8125f, 0.3125f }
    };
    float dither_value = dither_pattern
        [(pin.position.x) % 4]
        [(pin.position.y) % 4];
    float3 ray_current = ray_start + ray_step * dither_value;

    float step_current = dither_value * step_length;
    
    bool hit = true;
    //太陽方向を見ているときほど強く
    float cos_theta = saturate(dot(ray_dir, normalize(-light_direction.xyz)));
    //ミー散乱
    float phase = MiePhase(cos_theta, 0.4f);
    //補正
    phase *= 4.0f;
    //最低値
    
    phase = min(1.0f, max(phase,0.0f));
    float transmittance = 1.0f;
    float scattering = 0.0f;
    
    bool skip = false;
    //レイの現在地が霧の上限より高く、
    //かつ上向きのレイならば処理はしなくてよい
    if(ray_current.y>fog_max_height)
    {
        if(ray_step.y>0.f)
        {
            skip = true;
        }
    }
    
    if (!skip)
    {
        
        [loop]
        for (int i = 0; i < fog_steps; i++)
        {
            hit = true;
            
            //早期処理
            {
                //距離がオブジェクトの位置まで到達したら
                if(step_current>=obj_dis)
                    break;
                
                //レイがレイ高度を超えている場合
                if (ray_current.y > fog_max_height)//一定高度以上ならば
                {
                    //レイが上向きならば、
                    if (ray_step.y > 0.f)
                    {
                        break;
                    }
                
                    ray_current += ray_step;
                    continue;
                }
            }
        
            if (use_shadow > 0)
            {
                for (int j = 0; j < shadow_num;j++)
                {
            
                    float3 shadow_texcoord;
	                {
		                // ライトから見たNDC座標を算出
                        float4 wvpPos = mul(float4(ray_current, 1.0f), cascade_light_view_projection[j]);
		                // NDC座標からUV座標を算出する
                        wvpPos /= wvpPos.w;
                        wvpPos.y = -wvpPos.y;
                        wvpPos.xy = 0.5f * wvpPos.xy + 0.5f;
                        shadow_texcoord = wvpPos.xyz;
                    }
            
                    if (shadow_texcoord.z >= 0 && shadow_texcoord.z <= 1 &&
                                shadow_texcoord.x >= 0 && shadow_texcoord.x <= 1 &&
                                shadow_texcoord.y >= 0 && shadow_texcoord.y <= 1)
                    {
                        float depth = shadow_map[j].Sample(sampler_states[SHADOWMAP], shadow_texcoord.xy).r;

                        //深度値を比較して、影じゃなければ加算
                        if (shadow_texcoord.z - depth > shadow_bias)
                        {
                            hit = false;
                        }
                        break;
                    }
                }
            }
        

            {
                float density = SampleFogDensity(ray_current);
                        
                float optical_depth = density * step_length;
            
                //beer-lambert
                //本来：1.0f-exp(-optical_depth)
                //近似式 : float step_alpha = optical_depth / (1.0f + optical_depth);
                float step_alpha = 1.0f - exp(-optical_depth);
                
                //影の中なら弱くする
                float light_factor = hit ? 1.5f : 0.15f;
            
                scattering += transmittance * step_alpha * lerp(1.f,1.5f,phase) * light_factor;
            
                transmittance *= 1.0f - step_alpha;
            
                //早期処理
                if (transmittance <= 0.01f)
                    break;

            }
                    
            ray_current += ray_step;
            step_current += step_length;

        }

    }
    
    color = float4((scattering),0,0, object_depth);
    return color;
}