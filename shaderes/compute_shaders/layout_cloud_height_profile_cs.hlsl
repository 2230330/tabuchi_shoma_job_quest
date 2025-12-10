//雲の高度分布テクスチャの生成
RWTexture2D<float4> HeightProfile;

#define LAYOUT_CLOUD_HEIGHT_PROFILE_DIMENSIONS 64
#define LAYOUT_CLOUD_HEIGHT_PROFILE_NUMThREADS 8

[numthreads(LAYOUT_CLOUD_HEIGHT_PROFILE_NUMThREADS, LAYOUT_CLOUD_HEIGHT_PROFILE_NUMThREADS, LAYOUT_CLOUD_HEIGHT_PROFILE_NUMThREADS)]
void main(uint2 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= LAYOUT_CLOUD_HEIGHT_PROFILE_DIMENSIONS ||
        dtid.y >= LAYOUT_CLOUD_HEIGHT_PROFILE_DIMENSIONS)
    {
        return;
    }
    
    float h = (dtid.x) / float(LAYOUT_CLOUD_HEIGHT_PROFILE_DIMENSIONS); // 高度正規化
    float4 prof;
    
    // R: 層積（低層に集中）
    prof.r = smoothstep(0.05, 0.35, h) - smoothstep(0.55, 0.85, h);
    // G: 高層（中層中心）
    prof.g = smoothstep(0.25, 0.55, h) - smoothstep(0.75, 0.95, h);
    // B: 巻層（上層）
    prof.b = smoothstep(0.65, 0.85, h);
    // A: 乱層（広域）
    prof.a = smoothstep(0.10, 0.90, h);

    HeightProfile[uint2(dtid)] = saturate(prof);
}

