#ifndef YUZUKI_SHADERS_NOISE_FUNCTIONS_
#define YUZUKI_SHADERS_NOISE_FUNCTIONS_


//ランダム関数
float rand(float2 texel)
{
    return frac(sin(dot(texel, float2(12.9898f, 78.233f))) * 43758.5453f);
}
float rand2(float2 texel)
{
    texel = float2(dot(texel, float2(12.9898f, 78.233f)), dot(texel, float2(269.5f, 183.3f)));
    return -1.0f + 2.0f * frac(sin(texel) * float2(43758.5453123f, 43758.5453123f));
}

float3 rand3(float3 p)
{
    return frac(sin(float3(
        dot(p, float3(127.1, 311.7, 74.7)),
        dot(p, float3(269.5, 183.3, 246.1)),
        dot(p, float3(113.5, 271.9, 124.6))
    )) * 43758.5453);
}

float Hash13(float3 position)
{
    return frac(sin(dot(position, float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);

}


float Grad3D(float hash, float3 position)
{
    int h = int(1e4 * hash) & 15;
    float u = h < 8 ? position.x : position.y, v = h < 4 ? position.y : h == 12 || h == 14 ? position.x : position.z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    
    //int h = int(hash * 16.0f);
    //float3 grad = (h < 8) ? float3(1, 1, 0) : (h < 12) ? float3(1, 0, 1) : float3(0, 1, 1);
    //return dot(position, ((h & 1) == 0 ? grad : -grad));
}

//パーリンノイズ
//https://postd.cc/understanding-perlin-noise/
float PerlinNoise(float3 position, uint seed = 63456.0f)
{
    float3 pi = floor(position);
    float3 pf = position - pi;
    
    //smoothstepでもよい
    float u = pf.x * pf.x * pf.x * (pf.x * (6.0f * pf.x - 15.0f) + 10.0f);
    float v = pf.y * pf.y * pf.y * (pf.y * (6.0f * pf.y - 15.0f) + 10.0f);
    float w = pf.z * pf.z * pf.z * (pf.z * (6.0f * pf.z - 15.0f) + 10.0f);
    
    return lerp(lerp(lerp(Grad3D(Hash13(pi + float3(0, 0, 0)), pf - float3(0, 0, 0)),
                            Grad3D(Hash13(pi + float3(1, 0, 0)), pf - float3(1, 0, 0)),
     u),
    lerp(Grad3D(Hash13(pi + float3(0, 1, 0)), pf - float3(0, 1, 0)),
    Grad3D(Hash13(pi + float3(1, 1, 0)), pf - float3(1, 1, 0)),
    u),
    v)
    , lerp(lerp(Grad3D(Hash13(pi + float3(0, 0, 1)), pf - float3(0, 0, 1))
    , Grad3D(Hash13(pi + float3(1, 0, 1)), pf - float3(1, 0, 1))
    , u),
    lerp(Grad3D(Hash13(pi + float3(0, 1, 1)), pf - float3(0, 1, 1))
    , Grad3D(Hash13(pi + float3(1, 1, 1)), pf - float3(1, 1, 1)),
    u),
    v)
    , w);

}
//  https://qiita.com/Tanoren/items/0b83aa9ab69fc56bbeca
float PerlinNoiseFBM(float3 position, uint seed = 63456.0f, float adjust_value = 0.5f)
{
    float noise_sum = 0.0f,
		  frequency = 1.0f,
		  amplitude = 1.0f;

    static const int sOctaves = 5;
    for (int i = 0; i < sOctaves; ++i)
    {
        noise_sum += PerlinNoise(position * frequency) * amplitude;
        amplitude *= adjust_value;
        frequency *= rcp(adjust_value);
    }
    return noise_sum * 0.5f + 0.5f;
}

//https://www.cs.ubc.ca/~rbridson/docs/schechter-sca08-turbulence.pdf 整数用の簡易ハッシュ関数
uint EsgtsaOrig(uint s)
{
    s = (s ^ 2747636419u) * 2654435769;
    s = (s ^ (s >> 16)) * 2654435769u;
    s = (s ^ (s >> 16)) * 2654435769u;
    return s;
}
//元々は自然な煙のアニメーションを生成するための関数らしいが、簡易なランダム値生成にも使える
float Esgtsta(float4 v)
{
    return (EsgtsaOrig(EsgtsaOrig(EsgtsaOrig(EsgtsaOrig(uint(v.x)) + uint(v.y)) + uint(v.z)) + uint(v.w))) * 2.3283064365386962890625e-10f;
}

//https://andantesoft.hatenablog.com/entry/2024/12/19/193517
float ibuki(float4 v)
{
    const uint4 mult =
        uint4(0xae3cc725, 0x9fe72885, 0xae36bfb5, 0x82c1fcad);
  
    uint4 u = uint4(v);
    u = u * mult;
    u ^= u.wxyz ^ u >> 13;
      
    uint r = dot(u, mult);
  
    r ^= r >> 11;
    r = (r * r) ^ r;
          
    return r * 2.3283064365386962890625e-10;
}


float WorleyNoise3D(float3 p)
{
    float3 cell = floor(p);
    float minDist = 1.0;

    // 周囲のセルを探索
    int cell_range = 1;
    for (int z = -cell_range; z <= cell_range; z++)
    {
        for (int y = -cell_range; y <= cell_range; y++)
        {
            for (int x = -cell_range; x <= cell_range; x++)
            {
                float3 neighbor = cell + float3(x, y, z);
                float3 featurePoint = neighbor + rand3(neighbor);
                float dist = length(p - featurePoint);
                minDist = min(minDist, dist);
            }
        }
    }
    return minDist;
}


// FBM（Shadertoy風：オクターブ数多め）
float FBM(float3 p)
{
    float sum = 0.0;
    float amp = 0.5;
    float freq = 1.0;

    const int OCTAVES = 2; // Shadertoyは6～8が多い
    for (int i = 0; i < OCTAVES; i++)
    {
        sum += ibuki(float4(p * freq, 1.0f)) * amp;
        freq *= 2.0;
        amp *= 0.5;
    }
    return sum;
}


// Curlノイズ（流れを付ける）
float3 CurlNoise(float3 p)
{
    float eps = 0.1;
    float3 dx = float3(eps, 0, 0);
    float3 dy = float3(0, eps, 0);
    float3 dz = float3(0, 0, eps);

    float x = FBM(p + dy) - FBM(p - dy) - FBM(p + dz) + FBM(p - dz);
    float y = FBM(p + dz) - FBM(p - dz) - FBM(p + dx) + FBM(p - dx);
    float z = FBM(p + dx) - FBM(p - dx) - FBM(p + dy) + FBM(p - dy);

    return normalize(float3(x, y, z));
}

// Warp（座標を歪ませる）
float3 Warp(float3 p)
{
    float3 q = p + CurlNoise(p * 0.5) * 0.5;
    return q;
}


#endif// YUZUKI_SHADERS_NOISE_FUNCTIONS_