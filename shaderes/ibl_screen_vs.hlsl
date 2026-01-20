// フルスクリーン三角形頂点シェーダ

struct VSOut
{
    float4 posH : SV_Position;
    float2 uv : TEXCOORD0;
};

// フルスクリーン三角形（3頂点）
VSOut main(uint vid : SV_VertexID)
{
    VSOut o;
    // 3点で NDC 全面を覆う三角形
    //  ( -1, -1 ), ( -1, 3 ), ( 3, -1 )
    float2 pos[3] =
    {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0)
    };
    o.posH = float4(pos[vid], 0.0, 1.0);

    // 0..1 UV を PS で使って faceUV に変換（PS側で処理）
    o.uv = float2((pos[vid].x * 0.5 + 0.5), (-pos[vid].y * 0.5 + 0.5));
    return o;
}
