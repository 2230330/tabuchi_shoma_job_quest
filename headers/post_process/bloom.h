#pragma once
#include<d3d11.h>
#include<stdint.h>
#include<wrl.h>
#include<memory>

#include"../framebuffer.h"
#include"../resource_manager.h"
#include"../fullscreen_quad.h"

class Bloom
{
public:
    Bloom(ID3D11Device* device, uint32_t& width, uint32_t& height);

    ~Bloom() = default;
    Bloom(const Bloom&) = delete;
    Bloom& operator=(const Bloom&) = delete;
    Bloom(Bloom&&)noexcept = delete;
    Bloom& operator=(Bloom&&)noexcept = delete;

    void Make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetShaderResourceView()
    {
        return glow_extraction_->GetShaderResourceView(0);
    }

    //エミッシブマップをセットする関数
    void SetEmissiveMap(ID3D11ShaderResourceView* emissive_map) { emissive_map_ = emissive_map; }

    void DrawImgui();
private:
    //リソースマネージャ

    std::unique_ptr<FullscreenQuad>bit_block_transfer_;
    std::unique_ptr<FrameBuffer>glow_extraction_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emissive_map_;

    static const size_t downsampled_count = 6;
    std::unique_ptr<FrameBuffer>gaussian_blur[downsampled_count][2];

    Microsoft::WRL::ComPtr<ID3D11PixelShader>glow_extraction_ps_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>gaussian_blur_downsampling_ps_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>gaussian_blur_horizontal_ps_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>gaussian_blur_vertical_ps_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>gaussian_blur_upsampling_ps_;

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>depth_stencil_state_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>rasterizer_state_;
    Microsoft::WRL::ComPtr<ID3D11BlendState>blend_state_;

    struct BloomConstants
    {
        float bloom_extraction_threshold{1.0f};//明るさの閾値
        float bloom_intensity{1.5f};          //強度
        float bloom_soft_knee{0.3f};          //閾値付近の滑らかさ
        float bloom_radius{1.f}; //広がり
    };

    Microsoft::WRL::ComPtr<ID3D11Buffer>constant_buffer_;
    BloomConstants bloom_constant_{};

};