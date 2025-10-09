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
    Bloom(ID3D11Device* device, uint32_t& width, uint32_t& height, ResourceManager* resource_manager);

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

    void DrawImgui();
private:
    //リソースマネージャ
    ResourceManager* resource_manager_;

    std::unique_ptr<FullscreenQuad>bit_block_transfer_;
    std::unique_ptr<FrameBuffer>glow_extraction_;

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
        float bloom_extraction_threshold;//明るさの閾値
        float bloom_intensity;          //強度
        float bloom_soft_knee;          //閾値付近の滑らかさ
        float dummy;
    };

    Microsoft::WRL::ComPtr<ID3D11Buffer>constant_buffer_;
    BloomConstants bloom_constant_{};

};