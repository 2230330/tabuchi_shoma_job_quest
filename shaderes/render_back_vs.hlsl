#include "render_back.hlsli"

VS_OUT main(uint vertexID : SV_VertexID)
{
       // 3頂点で画面全体を覆う
    float2 position[3] =
    {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0)
    };

    VS_OUT output;
    output.position = float4(position[vertexID], 0.0, 1.0);
    output.uv = position[vertexID] * float2(0.5, -0.5) + 0.5;
    return output;
}