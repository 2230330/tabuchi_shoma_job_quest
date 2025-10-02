#include"../shaderes/fullscreen_quad.hlsli"

VS_OUT main(VS_INPUT inp)
{
    VS_OUT vout;
    const float2 position[4] = { {-1,+1},{+1,+1},{-1,-1},{+1,-1} };
    const float2 texcoords[4] = { {0,0},{1,0},{0,1},{1,1} };
    vout.position = float4(position[inp.vertexid], 0, 1);
    vout.texcoord = texcoords[inp.vertexid];

    return vout;
}