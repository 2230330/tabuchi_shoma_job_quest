#include"scene_constant_buffer.hlsli"
#include"forward_light.hlsli"
#include"fullscreen_quad.hlsli"
#include"camera_buffer.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
SamplerState sampler_states[6] : register(s0);

Texture2D cloud_texture : register(t0);

static const float PI = 3.14159265358979323846;


static const float3 WAVELENGTHS = float3(680.0f, 550.0f, 440.0f);
static const float3 INV_WAVELENGTH4 = (float3(
    1.0f / pow(WAVELENGTHS.x, 4.0f),
    1.0f / pow(WAVELENGTHS.y, 4.0f),
    1.0f / pow(WAVELENGTHS.z, 4.0f)
));

float3 ComputeSunIrradiance(float air_mass)
{
    float3 tau = INV_WAVELENGTH4 * air_mass * 3e9f;
    float3 Ei = exp(-tau);
    return Ei;
}

// 3x3 ボックスブラー + smoothstep によるソフトな雲カバレッジ（0..1）
float SampleSoftCloudCoverage(float2 uv)
{
    float2 texel = 1.0 / max(viewport_size.xy, float2(1e-6, 1e-6));
    const int R = 1; // 3x3
    float sum = 0.0;
    int count = 0;
    for (int y = -R; y <= R; ++y)
    {
        for (int x = -R; x <= R; ++x)
        {
            float2 off = float2(x, y) * texel;
            sum += cloud_texture.Sample(sampler_states[LINEAR_CLAMP], uv + off).r;
            count++;
        }
    }
    float avg = sum / (float) count;
    const float inner = 0.35;
    const float outer = 0.75;
    return saturate(smoothstep(inner, outer, avg));
}

float4 main(VS_OUT pin) : SV_Target
{
    // ソフトマスク（0..1）
    float softMask = SampleSoftCloudCoverage(pin.texcoord);

    // ビュー方向計算
    float4 ndc = float4(2.0 * pin.texcoord.x - 1.0, 1.0 - 2.0 * pin.texcoord.y, 0.0, 1.0);
    float4 pos = mul(ndc, inverse_view_projection_transform);
    pos /= pos.w;

    float3 view_dir = normalize(pos.xyz - camera_position.xyz);
    if (view_dir.y <= 0)
    {
        clip(-1); // 地平線より下は描かない)
    }

    float light_scale = 1.0f;
    
    float3 light_dir = normalize(-directional_light.direction.xyz);
    if(directional_light.direction.y >= 0)
    {
        light_dir = normalize(directional_light.direction.xyz);
        light_scale = 0.8f; // 月光は弱く
    }
    //float3 sun_dir = normalize(-directional_light.direction.xyz);
    //float3 moon_dir = normalize(directional_light.direction.xyz);
    
    // 安定した角度計算（スカラー）
    float cosTheta = clamp(dot(view_dir, light_dir), -0.0f, 1.0f);
    float angle = acos(cosTheta); // 0 = 視線と太陽が一致

    //ライン状ハイライト式
    const float EPS = 1e-4f;
    float mx = sin(abs(angle)) * 4.0;
    float my = mx;
    float denom_x = max(abs(angle) + my, EPS);
    float denom_y = max(abs(angle) + mx, EPS);
    float denom_xdiff = max(abs(angle - angle) + my, EPS); 
    float denom_ysum = max(abs(angle + angle) + mx, EPS);

    float lx = 0.01 / denom_x;
    float ly = 0.01 / denom_y;
    float lxy = 0.01 / denom_xdiff;
    float lyx = 0.01 / denom_ysum;
    float s = lx + ly + lxy + lyx;


    // 太陽の光（色・強度）
    float3 finalColor = (s.xxx);

    // 空気質量・太陽スペクトル
    if (directional_light.direction.y < 0)
    {
        float sun_elevation = clamp(dot(light_dir, float3(0, 1, 0)), 0.0f, 1.0f);
        float sun_theta = acos(sun_elevation) * (180.0f / PI);
        float air_mass = 1.0f / (sun_elevation + (0.50572f * pow(96.07995 - sun_theta, -1.6364)));
        float3 Ei = ComputeSunIrradiance(air_mass);
        
        finalColor *= Ei;
    }
    
    //雲による遮蔽計算
    float erosion = pow(softMask, 1.5f);
    //太陽を侵食
    float3 eroded_color = finalColor * (1.0f - erosion);

    // 太陽の強さに基づく安全なアルファ（黒点発生を避ける）
    // finalColor が小さくてもアルファがゼロに張り付かないようにする
    float sunIntensity = max(max(finalColor.r, finalColor.g), finalColor.b); // 最大チャネル
    // スケーリングは見た目に応じて調整（0.8~2.0 程度）
    float intensityScale = 1.;
    float colorAlpha = saturate(sunIntensity * intensityScale);

    // 侵食を考慮した最終アルファ
    float outAlpha = saturate(colorAlpha * (1.0 - erosion )); 

    // 非プリマルチ出力にする（レンダラーで通常アルファ合成を想定）
    float3 outColor =saturate( eroded_color*light_scale);

    return float4(outColor, outAlpha);
}