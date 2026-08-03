#pragma once
#pragma once
#include"i_render_system.h"

#include<d3d11.h>
#include<wrl.h>
#include<DirectXMath.h>
#include<memory>
#include<cstdint>

class ComponentManager;
class FullscreenQuad;

//画面に黄金分割を描画するシステム
//サムネ用の画像を描画するために使用します。
//黄金分割の描画は、フルスクリーン四角形を描画し、ピクセルシェーダーで黄金分割の線を描画することで実現します。    

class RenderGoldenRatioSystem :public IRenderSystem
{
public:
    RenderGoldenRatioSystem(ComponentManager& comp_mng, RenderPass render_pass);

    void Render()override;

private:

    ComponentManager& comp_mng_;

    //コンスタントバッファ
    struct GoldenRatioConstants
    {

        int orientation{ 0 };
        int dummy[3];
    };

    GoldenRatioConstants golden_ratio_constant_{};
    Microsoft::WRL::ComPtr<ID3D11Buffer> golden_ratio_constant_buffer_ = nullptr;

    //シェーダー
    Microsoft::WRL::ComPtr<ID3D11PixelShader>golden_ratio_ps_ = nullptr;

    //フルスクリーン描画
    std::unique_ptr<FullscreenQuad>fullscreen_quad_ = nullptr;

};