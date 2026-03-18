#include"gltf_model.hlsli"
#include"light_view_projection.hlsli"

VS_OUT main(INSTANCING_VS_IN vin)
{
    float sigma = vin.tangent.w;

    VS_OUT vout;

    float4x4 world_matrix;
    world_matrix._11_12_13_14 = vin.world0;
    world_matrix._21_22_23_24 = vin.world1;
    world_matrix._31_32_33_34 = vin.world2;
    world_matrix._41_42_43_44 = vin.world3;
    world_matrix = mul(world, world_matrix);

    vin.position.w = 1;
    float4 wpos = mul(vin.position, world_matrix);
    vout.position = mul(wpos, light_view_projection);
    //vout.w_position = mul(vin.position, world_matrix);

    //vin.normal.w = 0;
    //vout.w_normal = normalize(mul(vin.normal, world_matrix));

    //vin.tangent.w = 0;
    //vout.w_tangent = normalize(mul(vin.tangent, world_matrix));
    //vout.w_tangent.w = sigma;

    //vout.texcoord = vin.texcoord;

	
    return vout;
}
