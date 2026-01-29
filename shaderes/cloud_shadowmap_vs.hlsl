#include"scene_constant_buffer.hlsli"
#include"fullscreen_quad.hlsli"
#include"forward_light.hlsli"
VS_OUT main(float4 position : POSITION, float2 texcoord : TEXCOORD)
{   
    VS_OUT output = (VS_OUT) 0;
    output.position = mul(position, light_view_projection);
    output.texcoord = texcoord;
    return output;
}