#include"static_mesh.hlsli"

VS_OUT main(VS_INPUT inp)
{
    VS_OUT vout;

    vout.position = mul(inp.position, mul(inp.instance_world, view_projection));

    vout.world_position = mul(inp.position,inp.instance_world);
    inp.normal.w = 0;
    vout.world_normal = normalize(mul(inp.normal, inp.instance_world));

    vout.color = inp.instance_color;
    vout.texcoord = inp.texcoord;

    return vout;
}