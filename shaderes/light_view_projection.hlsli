//ライト情報、ライトのビュー射影行列、逆ライトのビュー射影行列を格納する定数バッファ
//ディファードレンダリングの情報に統合して、其々のライトで出せるようにしたい
//せっかくたくさんのライティングが出来るのに、一つだけでやるのは良くない

cbuffer ShadowSceneConstants : register(b6)
{
    row_major float4x4 light_view_projection;
    row_major float4x4 inverse_light_view_projection;
}
