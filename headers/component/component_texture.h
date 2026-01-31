#pragma once
#include"i_component.h"
#include<d3d11.h>
#include<wrl.h>
#include<string>
#include<memory>
struct ComponentTexture :public IComponent
{
    //テクスチャ自体を保持するのではなく、
    //テクスチャ配列のインデックスを保持する
    //現時点ではポインタ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
    //ちょっと邪道な気もしますが、識別用に名前を持たせます。
    std::string name;
};
