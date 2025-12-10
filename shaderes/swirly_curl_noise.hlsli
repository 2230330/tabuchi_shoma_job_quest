#ifndef __SWIRLY_CURL_NOISE_STATIC_HLSL__
#define __SWIRLY_CURL_NOISE_STATIC_HLSL__

// ---------------------------
// GLSL utility 置き換え
// ---------------------------
float frac1(float x)
{
    return x - floor(x);
}
float2 frac2(float2 x)
{
    return x - floor(x);
}
float3 frac3(float3 x)
{
    return x - floor(x);
}

// 回転（時間なしなので使わないが残しておく）
float2 RotCS(float2 p, float c, float s)
{
    return float2(p.x * c + p.y * s,
                 -p.x * s + p.y * c);
}

// ---------------------------
//  時間依存を削除した random3
// ---------------------------
float3 random3(float3 c)
{
    float j = 4096.0 * sin(dot(c, float3(17.0, 59.4, 15.0)));

    float3 r;
    r.z = frac1(512.0 * j);
    j *= .125;
    r.x = frac1(512.0 * j);
    j *= .125;
    r.y = frac1(512.0 * j);

    // Shadertoy ではここで r.xy を時間で回していたが削除
    // r.xy = Rot(r.xy, -iTime * 0.5);

    return r - 0.5;
}

// ---------------------------
// 3D Simplex Noise（時間依存削除版）
// ---------------------------
static const float F3 = 0.3333333;
static const float G3 = 0.1666667;

float noise(float3 p)
{
    float3 s = floor(p + dot(p, float3(F3, F3, F3)));
    float3 x = p - s + dot(s, float3(G3, G3, G3));

    float3 e = step(float3(0, 0, 0), x - x.yzx);
    float3 i1 = e * (1.0 - e.zxy);
    float3 i2 = 1.0 - e.zxy * (1.0 - e);

    float3 x1 = x - i1 + G3;
    float3 x2 = x - i2 + 2.0 * G3;
    float3 x3 = x - 1.0 + 3.0 * G3;

    float4 w;
    w.x = dot(x, x);
    w.y = dot(x1, x1);
    w.z = dot(x2, x2);
    w.w = dot(x3, x3);
    w = max(0.6 - w, 0.0);

    float4 d;
    d.x = dot(random3(s), x);
    d.y = dot(random3(s + i1), x1);
    d.z = dot(random3(s + i2), x2);
    d.w = dot(random3(s + 1.0), x3);

    w *= w;
    w *= w;
    d *= w;

    return dot(d, float4(52.0, 52.0, 52.0, 52.0));
}

// ---------------------------
// 2D simplex noise（時間依存削除版）
// ---------------------------
float2 hash2(float2 p)
{
    p = float2(
        dot(p, float2(127.1, 311.7)),
        dot(p, float2(269.5, 183.3))
    );

    // 時間で回す操作を削除
    return -1.0 + 2.0 * frac2(sin(p) * 43758.5453123);
}

float noise(float2 p)
{
    const float K1 = 0.366025404;
    const float K2 = 0.211324865;

    float2 i = floor(p + (p.x + p.y) * K1);

    float2 a = p - i + (i.x + i.y) * K2;
    float2 o = (a.x > a.y) ? float2(1, 0) : float2(0, 1);

    float2 b = a - o + K2;
    float2 c = a - 1.0 + 2.0 * K2;

    float3 h = max(0.5 - float3(dot(a, a), dot(b, b), dot(c, c)), 0.0);
    h = h * h;
    h = h * h;

    float3 n = float3(
        dot(a, hash2(i + float2(0, 0))),
        dot(b, hash2(i + o)),
        dot(c, hash2(i + float2(1, 1)))
    );

    return dot(n, float3(70, 70, 70));
}

// ---------------------------
// Curl Noise（時間要素なし）
// ---------------------------
float3 CurlNoise(float3 p)
{
    float eps = 0.1;

    float ny = noise(p + float3(0, eps, 0));
    float py = noise(p - float3(0, eps, 0));

    float nz = noise(p + float3(0, 0, eps));
    float pz = noise(p - float3(0, 0, eps));

    float nx = noise(p + float3(eps, 0, 0));
    float px = noise(p - float3(eps, 0, 0));

    float x = ny - nz - (py - pz);
    float y = nz - nx - (pz - px);
    float z = nx - py - (px - py);

    return normalize(float3(x, y, z));
}

// ---------------------------
// Octave Curl（時間変化削除版）
// ---------------------------
float3 CurlOctave(float3 p, float scale, float strength)
{
    p *= scale;
    return CurlNoise(p) * strength;
}




float3 FlowField(float3 p)
{
    float3 f = 0;

    // 大きなうねり（弱め）
    float3 largeCurl = CurlNoise(p * 0.02) * 1.2;
    p += largeCurl;

    // 風方向のバイアス（弱め）
    float3 windDir = normalize(float3(-1.0, -0.2, 0.0));
    p += windDir * 2.0;

    // オクターブ構成（低周波→高周波）
    p *= 0.08;
    f += CurlOctave(p, 2.0f, 1.0);
    p *= 2.5;
    f += CurlOctave(p, 2.0f, 0.7);
    p *= 3.0;
    f += CurlOctave(p, 2.0f, 0.4);
    p *= 3.5;
    f += CurlOctave(p, 2.0f, 0.2);
    p *= 4.0;
    f += CurlOctave(p, 2.0f, 0.1); // 高周波追加

    // 強度を残して非線形で強調
    float lenF = length(f);
    return normalize(f) * pow(lenF, 1.2); // 渦の強さをさらに残す
}



#endif
