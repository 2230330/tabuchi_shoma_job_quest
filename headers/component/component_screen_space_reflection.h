#pragma once
#include"i_component.h"
#include<d3d11.h>
#include<wrl.h>

struct ComponentSsr :public IComponent
{
    //スクリーンスペースリフレクションのパラメータ
    //反射の強さなどを調整するためのパラメータを入れる予定
    float distance{ 10.0f };
    int num_steps{ 10 };
    int max_mip{ 6 };
    float thickness{ 0.5f };
    float resolution{ 0.3f };
    float start_bias{ 0.05f };


    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ssr_texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normal;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> color;
};