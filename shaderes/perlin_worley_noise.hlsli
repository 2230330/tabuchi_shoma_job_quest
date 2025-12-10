// Lightweight tileable 3D Perlin-Worley noise (差し替え用)
// オリジナル Shadertoy 実装を軽量化した版
// - hash33_fast：軽量ハッシュ（normalize 省略）
// - Worley：探索を 19 サンプルに削減
// - FBM：軽量ハッシュ hash1 を利用
// - CurlNoiseFast：差分ではなく擬似的な流れ表現 (3 サンプル)

#ifndef __LIGHTWEIGHT_PERLIN_WORLEY__
#define __LIGHTWEIGHT_PERLIN_WORLEY__

// ---- ユーティリティ ----
float remap(float x, float a, float b, float c, float d)
{
    return (((x - a) / (b - a)) * (d - c)) + c;
}


static const uint UI0 = 1597334673u;
static const uint UI1 = 3812015801u;
static const uint3 UI3 = uint3(UI0, UI1, 2798796415u);
// 1.0 / float(0xffffffffU) と同値。HLSLでは直接リテラルでOK。
static const float UIF = 1.0 / 4294967295.0;

float3 hash33(float3 p)
{
    // GLSL: uvec3(ivec3(p)) に相当
    // HLSLでは (int3)p → (uint3) にキャスト
    uint3 q = (uint3) ((int3) p) * UI3;

    // 3成分の XOR をスカラに畳み、それを UI3 にブロードキャストして乗算
    q = ((q.x ^ q.y ^ q.z)) * UI3;

    // uint3 → float3 にキャストして正規化し、[-1, +1] に拡張
    return -1.0 + 2.0 * (float3) q * UIF;
}

// 軽量 hash -> [-1,1] 範囲を返す（高速）
float3 hash33_fast(float3 p)
{
    // frac/dot を使った高速な近似ハッシュ
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    float3 r = frac(float3(
        p.x * p.y + p.z,
        p.x + p.y * p.z,
        p.x * p.z + p.y
    ));
    return r * 2.0f - 1.0f;
}

// 単純なスカラーハッシュ（FBM 用）
float hash1(float3 p)
{
    return frac(sin(dot(p, float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);

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

// quintic 補間
float3 quintic(float3 w)
{
    return w * w * w * (w * (w * 6.0f - 15.0f) + 10.0f);
}

// ---- Gradient (Perlin-like) noise (tileable) ----
float gradient_noise(float3 x, float freq)
{
    float3 p = floor(x);
    float3 w = frac(x);
    float3 u = quintic(w);

    // グラディエントを軽量ハッシュで取得
    float3 ga = hash33(fmod(p + float3(0.0f, 0.0f, 0.0f), freq));
    float3 gb = hash33(fmod(p + float3(1.0f, 0.0f, 0.0f), freq));
    float3 gc = hash33(fmod(p + float3(0.0f, 1.0f, 0.0f), freq));
    float3 gd = hash33(fmod(p + float3(1.0f, 1.0f, 0.0f), freq));
    float3 ge = hash33(fmod(p + float3(0.0f, 0.0f, 1.0f), freq));
    float3 gf = hash33(fmod(p + float3(1.0f, 0.0f, 1.0f), freq));
    float3 gg = hash33(fmod(p + float3(0.0f, 1.0f, 1.0f), freq));
    float3 gh = hash33(fmod(p + float3(1.0f, 1.0f, 1.0f), freq));

    float va = dot(ga, w - float3(0.0f, 0.0f, 0.0f));
    float vb = dot(gb, w - float3(1.0f, 0.0f, 0.0f));
    float vc = dot(gc, w - float3(0.0f, 1.0f, 0.0f));
    float vd = dot(gd, w - float3(1.0f, 1.0f, 0.0f));
    float ve = dot(ge, w - float3(0.0f, 0.0f, 1.0f));
    float vf = dot(gf, w - float3(1.0f, 0.0f, 1.0f));
    float vg = dot(gg, w - float3(0.0f, 1.0f, 1.0f));
    float vh = dot(gh, w - float3(1.0f, 1.0f, 1.0f));

    // 三次元補間（簡潔化した式）
    float ixy = va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * (va - vb - vc + vd);
    float iz = ve + u.x * (vf - ve) + u.y * (vg - ve) + u.x * u.y * (ve - vf - vg + vh);
    return ixy + u.z * (iz - ixy);
}

// Perlin風 FBM（octaves は小さめ推奨）
float perlin_fbm(float3 p, float freq, int octaves)
{
    float g = exp2(-0.85f);
    float amp = 1.0f;
    float noise = 0.0f;
    for (int i = 0; i < octaves; ++i)
    {
        noise += amp * gradient_noise(p * freq, freq);
        freq *= 2.0f;
        amp *= g;
    }
    return noise;
}

// ---- Tileable Worley noise（19 サンプル） ----
// 19 サンプルのオフセット集合（対称性を保ちつつ数を削減）
static const int3 WORLEY_OFFSETS[19] =
{
    int3(0, 0, 0),
    int3(1, 0, 0), int3(-1, 0, 0),
    int3(0, 1, 0), int3(0, -1, 0),
    int3(0, 0, 1), int3(0, 0, -1),
    int3(1, 1, 0), int3(-1, -1, 0),
    int3(1, -1, 0), int3(-1, 1, 0),
    int3(1, 0, 1), int3(-1, 0, -1),
    int3(1, 0, -1), int3(-1, 0, 1),
    int3(0, 1, 1), int3(0, -1, -1),
    int3(0, 1, -1), int3(0, -1, 1)
};

float worley_noise(float3 uv, float freq)
{
    float3 id = floor(uv);
    float3 p = frac(uv);

    float min_dist = 10000.f;
    [unroll]
    for (int i = 0; i < 19; ++i)
    {
        float3 offset = float3(WORLEY_OFFSETS[i]);
        // freq でのタイル処理（fmod でループ）
        float3 cell = fmod(id + offset, freq);
        // 0..1 のランダム点（hash -> [ -1,1 ] を [0,1] に）
        float3 h = hash33(cell) * 0.5f + 0.5f;
        h += offset;
        float3 d = p - h;
        float dd = dot(d, d);
        min_dist = min(min_dist, dd);
    }

    // inverted worley（元実装にならって）
    return (1.0f - min_dist);
}


// 0..1 のジッタ
float3 jitter01(uint3 cellId)
{
    return 0.5 * (hash33(cellId) + 1.0); // [-1,1]→[0,1]
}

float worley_noise_fixed(float3 uv, int period /*= (int)freq*/)
{
    float3 idf = floor(uv);
    int3 cid = (int3) idf; // 整数セルID
    float3 p = frac(uv); // 局所座標 0..1

    float minDist2 = 1e10;

    // 3×3×3 探索
    [unroll]
    for (int oz = -1; oz <= 1; ++oz)
        for (int oy = -1; oy <= 1; ++oy)
            for (int ox = -1; ox <= 1; ++ox)
            {
                int3 nid = cid + int3(ox, oy, oz);

        // タイル（モジュロ）: 正の範囲に正規化
                if (period > 0)
                {
                    nid.x = (nid.x % period + period) % period;
                    nid.y = (nid.y % period + period) % period;
                    nid.z = (nid.z % period + period) % period;
                }

        // ハッシュ入力は整数セルID
                float3 j = jitter01((uint3) nid); // 0..1

        // 特徴点の局所位置 = offset + jitter
                float3 feature = float3(ox, oy, oz) + j;

        // 距離は p(0..1) からの差
                float3 d = p - feature;
                float d2 = dot(d, d);
                minDist2 = min(minDist2, d2);
            }

    // inverted worley（F1相当）
    return 1.0 - minDist2; // あるいは 1.0 - sqrt(minDist2) を好みに応じて
}


float worley_fbm(float3 p, float freq)
{
    // freq は最初の周波数（例: 1.0）
    return worley_noise_fixed(p * freq, freq) * 0.625f +
           worley_noise_fixed(p * freq * 2.0f, freq * 2.0f) * 0.25f +
           worley_noise_fixed(p * freq * 4.0f, freq * 4.0f) * 0.125f;
}

// ---- Shadertoy 風 FBM（軽量 hash1 を使用） ----
float FBM(float3 p)
{
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;

    const int OCTAVES = 2; // 軽量化のため小さい値に
    for (int i = 0; i < OCTAVES; ++i)
    {
        // hash1 を利用した簡易ノイズ（厳密な gradient ノイズではないが高速）
        sum += hash1(p * freq) * amp;
        freq *= 2.0f;
        amp *= 0.5f;
    }
    return sum;
}

// ---- 疑似 Curl（差分を避けた軽量版） ----
float3 CurlNoiseFast(float3 p)
{
    // 少数のハッシュ呼び出しで流れを作る
    float a = hash1(p + float3(12.34f, 45.67f, 78.90f));
    float b = hash1(p + float3(98.76f, 54.32f, 11.11f));
    float c = hash1(p + float3(21.21f, 43.43f, 65.65f));
    float3 v = float3(a, b, c) * 2.0f - 1.0f;
    return normalize(v);
}

// ---- Warp（軽量） ----
float3 Warp(float3 p)
{
    // CurlNoiseFast を小さくスケールして座標を歪ませる
    return p + CurlNoiseFast(p * 0.5f) * 0.4f;
}

#endif // __LIGHTWEIGHT_PERLIN_WORLEY__
