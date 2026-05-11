#pragma once
#include<d3d11.h>
#include<stdint.h>
#include<wrl.h>
#include<memory>


//前方宣言
class FrameBuffer;
class FullscreenQuad;

//ブルーム処理を行うクラス。
//現在は散乱方向を少なくした軽いものを使っています。
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

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetShaderResourceView();


    //エミッシブマップをセットする関数
    void SetEmissiveMap(ID3D11ShaderResourceView* emissive_map) { emissive_map_ = emissive_map; }

    void DrawImgui();
private:
    //リソースマネージャ

    std::unique_ptr<FullscreenQuad>bit_block_transfer_=nullptr;
    std::unique_ptr<FrameBuffer>glow_extraction_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emissive_map_ = nullptr;

    static const size_t downsampled_count = 6;
    std::unique_ptr<FrameBuffer>gaussian_blur[downsampled_count][2];

    Microsoft::WRL::ComPtr<ID3D11PixelShader>glow_extraction_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>gaussian_blur_downsampling_ps_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>gaussian_blur_horizontal_ps_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>gaussian_blur_vertical_ps_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>gaussian_blur_upsampling_ps_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>depth_stencil_state_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>rasterizer_state_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11BlendState>blend_state_ = nullptr;

    struct BloomConstants
    {
        float bloom_extraction_threshold{2.0f};//明るさの閾値
        float bloom_intensity{1.5f};          //強度
        float bloom_soft_knee{0.3f};          //閾値付近の滑らかさ
        float bloom_radius{1.f}; //広がり
    };

    Microsoft::WRL::ComPtr<ID3D11Buffer>constant_buffer_ = nullptr;
    BloomConstants bloom_constant_{};

};