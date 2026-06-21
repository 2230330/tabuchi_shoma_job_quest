Texture2D<float> src : register(t0);
RWTexture2D<float> dst : register(u0);


[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    //出力ピクセル位置に対応する入力座標を入れる2＊2
    uint2 texel = id.xy * 2;
    
    float d0 = src.Load(int3(texel, 0));
    float d1 = src.Load(int3(texel + uint2(1, 0), 0));
    float d2 = src.Load(int3(texel + uint2(0, 1), 0));
    float d3 = src.Load(int3(texel + uint2(1, 1), 0));

    float min_depth = min(min(d0, d1), min(d2, d3));

    dst[id.xy] = min_depth;
}