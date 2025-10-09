#pragma once
#include<d3d11.h>
#include<stdint.h>
#include<wrl.h>
#include<memory>

#include"../resource_manager.h"
#include"bloom.h"
#include"../fullscreen_quad.h"


    //ポストプロセスを一か所で纏める為のクラス
class PostProcessManager
{
public:
    PostProcessManager(ID3D11Device* device,uint32_t&width,uint32_t&height,
        ResourceManager* resource_manager);

    void PostProcess(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map);

    void PostImgui();

private:
    ResourceManager* resource_manager_;
    std::unique_ptr<FullscreenQuad> result_transfer_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>result_synthesiser_;

    //以下、ポストプロセス
    std::unique_ptr<Bloom> bloom_;

};