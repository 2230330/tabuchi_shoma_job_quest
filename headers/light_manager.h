#ifndef PART2_LIGHT_MANAGER_H
#define PART2_LIGHT_MANAGER_H

#include<DirectXMath.h>
#include<d3d11.h>
#include<wrl.h>
#include<vector>

struct DirectionLight
{
    DirectX::XMFLOAT4    direction{ 0.f,.0f,-1.f,1.f };
    DirectX::XMFLOAT4    color{ 1.f,1.f,1.f,1.f };
    float intensity{ 1.0f };
    DirectX::XMFLOAT3 dummy;
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

    //ライトのIMGUI管理
    void DrawImgui();

private:
    DirectionLight direction_light_;

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
        DirectionLight directional_light;
        PointLight point_light[light_max];
        SpotLight spot_light[light_max];

    };
};

#endif //!PART2_LIGHT_MANAGER_H