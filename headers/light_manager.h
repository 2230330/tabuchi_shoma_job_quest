#ifndef PART2_LIGHT_MANAGER_H
#define PART2_LIGHT_MANAGER_H

#include<DirectXMath.h>
#include<d3d11.h>
#include<wrl.h>
#include<vector>

struct DirectionLight
{
    DirectX::XMFLOAT4    direction{ 0.f,-1.0f,0.f,1.f };
    DirectX::XMFLOAT4    color{ 1.f,1.f,1.f,1.f };//color.wが強度

};
struct PointLight
{
    DirectX::XMFLOAT4 position{ 0.f,0.f,0.f,0.f };
    DirectX::XMFLOAT4 color{ 1.f,1.f,1.f,1.f };
    float range{ 0.f };
    float intensity{ 1.0f };
    DirectX::XMFLOAT2 dummy;
};
struct SpotLight
{
    DirectX::XMFLOAT4 position{ 0.f,0.f,0.f,0.f };
    DirectX::XMFLOAT4 direction{ 0.f,0.f,-1.f,0.f };
    DirectX::XMFLOAT4 color{ 1.f,1.f,1.f,1.f };
    float range{ 0.f };
    float inner_corn{ DirectX::XMConvertToRadians(30.f) };
    float outer_corn{ DirectX::XMConvertToRadians(45.f) };
    float dummy;
};

class LightManager
{
public:
    //コンスタント
    LightManager();

    //ディレクションライト設定
    void SetDirectionLight(DirectionLight& light) { this->direction_light_ = light; }

    //ディレクションライト取得
    DirectionLight GetDirectionLight()const { return this->direction_light_; }

    //ライトコンスタントを楽に送り出せるように
    void SetForwardLightConstant(int start_slot);

    //デファード用ライトコンスタントをセット
    void SetDeferredLightConstant(int start_slot);

    //ライトのIMGUI管理
    void DrawImgui();

    void Update(float elampsed_time);

private:
    bool moove_light_ = false;
    float day_length_seconds_ = 120.0f;//時間経過のスケール
    float sun_tilt_ = DirectX::XMConvertToRadians(23.5f);//太陽の傾き
    float time_of_day = 0.5f;//0~1で1日を表現

    DirectionLight direction_light_;
    float azimuth_ = 0.f;//ライトの水平角度
    float elevation_ = DirectX::XMConvertToRadians(-90);//ライトの仰角
    DirectX::XMFLOAT4 ambient_color_ = { 1,1,1,1 };
    //ライト空間用
    DirectX::XMFLOAT4X4 light_view_projection_{};
    DirectX::XMFLOAT4X4 inverse_light_view_projection_{};
    const float shadow_distance_ = 50000;
    const float shadow_near_plane_ = 1.0f;
    const float shadow_far_plane_ = 200000;
    const float shadow_map_size_ = 1024.0f;


    std::vector<PointLight>point_lights_;
    std::vector<SpotLight>spot_lights_;

    Microsoft::WRL::ComPtr<ID3D11Buffer>forward_light_constant_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>deferred_light_constant_buffer_;

    struct ForwardLightConstants
    {
        static constexpr int light_max = 8;
        DirectX::XMFLOAT4 ambient_color;
        //ｘ：空き、ｙ：ポイントライト数、ｚ：スポットライト数、ｗ：空き
        DirectX::XMUINT4  light_count{ 0,0,0,0 };
        DirectX::XMFLOAT4X4 light_view_position;//ライトビュー空間でのカメラ位置
        DirectX::XMFLOAT4X4 inverse_light_view_position;//逆行列
        DirectX::XMFLOAT2 light_orthographic_size;//ライトの直交投影のサイズ(x:width,y:height)
        DirectX::XMFLOAT2 light_depth_range;//ライトの直交投影の深度範囲(x:near,y:far)
        DirectionLight directional_light;
        PointLight point_light[light_max];
        SpotLight spot_light[light_max];

    };

    const int light_kind_derectional_light = 0;
    const int light_kind_point_light = 1;
    const int light_kind_spot_light = 2;
    const int light_kind_ambient_light = 3;

    struct IntegrateLight
    {
        //共有の情報
        //work_data[3].wは共有のライトの種別番号を入れておく

        //平行光源の場合
        //work_data[0]=direction
        //work_data[1]=color
        //work_data[2]=dummy
        //work_data[3]=xyz=dummy,w=ライト識別番号

        //点光源
        //work_data[0]=position
        //work_data[1]=color
        //work_data[2]=x=range,yzw=dummy
        //work_data[3]=xyz=dummy,w=ライト識別番号

        //スポットライトの場合
        //work_data[0]=position
        //work_data[1]=direction
        //work_data[2]=color
        //work_data[3]=x=range,y=inner_cone,z=outer_cone,w=ライト識別番号

        //環境光の場合
        //work_data[0]=color
        //work_data[1]=dummy
        //work_data[2]=dummy
        //work_data[3]=xyz=dummy,w=ライト識別番号
        DirectX::XMFLOAT4 work_data[4];
    };
    struct DeferredLightContstants
    {
        IntegrateLight lights;

        //シャドウマップ関係
        int use_shadow{ 0 };//　影を擁しているかどうか
        float shadow_attenuation{ 0.5f };//影色
        float shadow_bias{ 0.001f };//深度バイアス
        UINT shadow_dummy{0};
        DirectX::XMFLOAT4X4 light_view_projection{}; //ライトの位置から見た射影行列
    };
};

#endif //!PART2_LIGHT_MANAGER_H