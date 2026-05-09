#include"gltf_model.hlsli"
#include"light_view_projection.hlsli"

VS_OUT main(VS_IN vin)
{
    float sigma = vin.tangent.w;

#if 1
	// UNIT.37
    if (skin > -1)
    {
        row_major float4x4 skin_matrix =
        vin.weights.x * joint_matrices[vin.joints.x]
        + vin.weights.y * joint_matrices[vin.joints.y]
        + vin.weights.z * joint_matrices[vin.joints.z]
        + vin.weights.w * joint_matrices[vin.joints.w];
        vin.position = mul(float4(vin.position.xyz, 1), skin_matrix);
        vin.normal = normalize(mul(float4(vin.normal.xyz, 0), skin_matrix));
        vin.tangent = normalize(mul(float4(vin.tangent.xyz, 0), skin_matrix));
    }
#endif

    VS_OUT vout;

    vin.position.w = 1;
    float4 wpos = mul(vin.position, world);
    vout.position = mul(wpos, cascade_light_view_projection[current_index]);
 //   vout.w_position = mul(vin.position, world);

 //   vin.normal.w = 0;
 //   vout.w_normal = normalize(mul(vin.normal, world));

	////float sigma = vin.tangent.w;
 //   vin.tangent.w = 0;
 //   vout.w_tangent = normalize(mul(vin.tangent, world));
 //   vout.w_tangent.w = sigma;

 //   vout.texcoord = vin.texcoord;

    return vout;
}