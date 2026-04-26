#pragma once
#include"i_component.h"
#include<d3d11.h>
#include<wrl.h>

struct ComponentSsr :public IComponent
{
    //スクリーンスペースリフレクションのパラメータ
    //反射の強さなどを調整するためのパラメータを入れる予定

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ssr_texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normal;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> color;
};