#include"scene_constant_buffer.hlsli"
#include"forward_light.hlsli"
#include"fullscreen_quad.hlsli"

#define POINT_WRAP 0
#define POINT_CLAMP 1
#define LINEAR_WRAP 2
#define LINEAR_CLAMP 3
#define ANISOTROPIC 4
#define LINEAR_MIRROR 5
SamplerState sampler_states[6] : register(s0);

Texture2D cloud_texture : register(t0);

static const float PI = 3.14159265358979323846;


float3 hsv2rgb(float3 c)
{
    // GLSL: abs(mod(c.x * 6.0 + vec3(0,4,2),6.0)-3.0)-1.0
    float3 t = abs(fmod(c.x * 6.0 + float3(0.0, 4.0, 2.0), 6.0) - 3.0);
    float3 rgb = saturate(t - 1.0); // clamp( .. , 0, 1 ) の代替として saturate
    return c.z * lerp(float3(1.0, 1.0, 1.0), rgb, c.y);
}

// 2D 回転行列
float2x2 rotate2d(float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    // GLSL mat2 と同じ並び
    return float2x2(c, -s,
                    s, c);
}


// 波長 (nm)
static const float3 WAVELENGTHS = float3(680.0f, 550.0f, 440.0f); // 赤, 緑, 青
// λ^-4 の相対比を計算
static const float3 INV_WAVELENGTH4 = (float3(
    1.0f / pow(WAVELENGTHS.x, 4.0f),
    1.0f / pow(WAVELENGTHS.y, 4.0f),
    1.0f / pow(WAVELENGTHS.z, 4.0f)
));

float3 ComputeSunIrradiance(float air_mass)
{
    // 光学的厚さ τ(λ)
    float3 tau = INV_WAVELENGTH4 * air_mass * 3e9f /*スキャッタリングスケール*/;

    // Beer-Lambert減衰
    float3 Ei = exp(-tau);

    return Ei; // 赤が残り、青が強く減衰
}

//float4 main(VS_OUT pin) : SV_TARGET
//{
//    float4 ndc = float4(2.0 * pin.texcoord.x - 1.0, 1.0 - 2.0 * pin.texcoord.y, 0.0, 1.0);
//    float4 pos = mul(ndc, inverse_view_projection_transform);
//    pos /= pos.w;
    
//    float3 view_dir = normalize(pos.xyz - camera_position.xyz);
//    if(view_dir.y<=0)
//    {
//        return float4(0, 0, 0, 0);
//    }
    
//    float3 light_dir = normalize(-directional_light.direction.xyz);
//    float zenith_angle = clamp(dot(float3(0.0, 1.0, 0.0), light_dir), -1.0, 1.0);
//    float sun_energy = max(0.0, 1.0 - exp(-((PI * 0.5) - acos(zenith_angle)))) * 1000.0;
    
//    float4 color = float4(0, 0, 0, 0);
    
//        //太陽
//    {
//        const float sol_size = 0.00872663806;
//        const float sun_disk_scale = 2.0; // [0.0, 360.0]
//	    // solar disk and out-scattering
//        float sun_angular_diameter_cos_min = cos(sol_size * sun_disk_scale);
//        float sun_angular_diameter_cos_max = cos(sol_size * sun_disk_scale * 0.5);
        
//        float cos_theta = clamp(dot(view_dir, light_dir), -1.0f, 1.0f); //視線と太陽の角度
//        float sun_elevation = clamp(dot(light_dir, float3(0, 1, 0)), 0.0f, 1.0f); // 太陽の高さ
//        float sun_theta = acos(sun_elevation) * (180.0f / PI); //度に変換
//    //kasten-Young 1989近似
//        float air_mass = 1.0f / (sun_elevation + (0.50572f * pow(96.07995 - sun_theta, -1.6364))); // secant近似
//        float3 Ei = ComputeSunIrradiance(air_mass); //太陽光の色
        
//        float sun_disk = smoothstep(sun_angular_diameter_cos_min, sun_angular_diameter_cos_max, cos_theta);
//        float3 Lo =  sun_disk * Ei;
//        // 太陽のディスク内に視線が入っているときだけ加算
//        if (sun_disk > 0.01f) // しきい値で完全に限定
//        {
//            color.rgb += Lo;
//        }
//    }
    
//    return color;
//}


// ピクセルシェーダ（Shadertoy の mainImage 相当）
float4 main(VS_OUT pin) : SV_Target
{
    float4 enable_cloud = cloud_texture.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
    if(enable_cloud.r > 0.1)
    {
        return float4(0,0,0,0);
    }
    
    float4 ndc = float4(2.0 * pin.texcoord.x - 1.0, 1.0 - 2.0 * pin.texcoord.y, 0.0, 1.0);
    float4 pos = mul(ndc, inverse_view_projection_transform);
    pos /= pos.w;

    float3 view_dir = normalize(pos.xyz - camera_position.xyz);
    if (view_dir.y <= 0)
    {
        return float4(0, 0, 0, 0);
    }
    
    float3 light_dir = normalize(-directional_light.direction.xyz);
    
    float3 angular_dis = acos(clamp(dot(view_dir, light_dir), 0.0, 1.0));
    // ------------------------------
    // 光の玉（ラインと交差のようなハイライト）
    // ------------------------------
    float s;
    float mx = sin(abs(angular_dis.x)) * 2.; // length(coord.x) == abs(coord.x)
    float my = sin(abs(angular_dis.y)) * 2.;

    float lx = 0.01 / abs(abs(angular_dis.x) + my);
    float ly = 0.01 / abs(abs(angular_dis.y) + mx);
    float lxy = 0.01 / abs(abs(angular_dis.x - angular_dis.y) + my);
    float lyx = 0.01 / abs(abs(angular_dis.y + angular_dis.x) + mx);
    s = lx + ly + lxy + lyx;

    // ------------------------------
    // 虹の輪（ドーナツ状のカラーリング）
    // ------------------------------
    float l = length(angular_dis);
    float hue = (l / PI) * 10.0 - options.z * 0.1;

    
    // ドーナツ形状のパラメータ
    float inner = 0.6;
    float outer = 0.9;
    float blur = 0.4;

    // smoothstep を使った内外半径のブレンド
    float ring =
        smoothstep(inner, inner + blur, l) *
        (1.0 - smoothstep(outer - blur, outer, l));

    // HSV（H=hue, S=1, V=ring）→ RGB
    float3 color = hsv2rgb(float3(hue, 1.0, ring));
    
    float cos_theta = clamp(dot(view_dir, light_dir), -1.0f, 1.0f); //視線と太陽の角度
    float sun_elevation = clamp(dot(light_dir, float3(0, 1, 0)), 0.0f, 1.0f); // 太陽の高さ
    float sun_theta = acos(sun_elevation) * (180.0f / PI); //度に変換
    //kasten-Young 1989近似
    float air_mass = 1.0f / (sun_elevation + (0.50572f * pow(96.07995 - sun_theta, -1.6364))); // secant近似
    float3 Ei = ComputeSunIrradiance(air_mass); //太陽光の色

    // 最終カラー：虹の輪 + 光の玉の足し合わせ
    float3 finalColor = (Ei*color) + s.xxx;

    // アルファ値をカラーの平均値に設定
    float alpha = enable_cloud.r;
    
    return float4(finalColor, alpha);
}
