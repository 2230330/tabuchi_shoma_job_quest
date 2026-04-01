//ライト情報、ライトのビュー射影行列、逆ライトのビュー射影行列を格納する定数バッファ
//ディファードレンダリングの情報に統合して、其々のライトで出せるようにしたい
//せっかくたくさんのライティングが出来るのに、一つだけでやるのは良くない

cbuffer LightViewProjection : register(b6)
{
    row_major float4x4 light_view_projection;
    row_major float4x4 inverse_light_view_projection;
}

cbuffer CascadeShadowMap : register(b8)
{
    row_major float4x4 cascade_light_view_projection[4];
    row_major float4x4 cascade_inverse_light_view_projection;
}