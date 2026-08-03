#pragma once

#include <d3d11.h>
#include <stdint.h>
#include <wrl.h>
#include <array>

// ブルーム処理を行うクラス。
// Compute Shader版。
class Bloom
{
public:
    Bloom(ID3D11Device* device, uint32_t& width, uint32_t& height);

    ~Bloom() = default;
    Bloom(const Bloom&) = delete;
    Bloom& operator=(const Bloom&) = delete;
    Bloom(Bloom&&) noexcept = delete;
    Bloom& operator=(Bloom&&) noexcept = delete;

    // ブルーム処理を行う関数。
    void Make(
        ID3D11DeviceContext* immediate_context,
        ID3D11ShaderResourceView* color_map
    );

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetShaderResourceView();

    void SetEmissiveMap(ID3D11ShaderResourceView* emissive_map)
    {
        emissive_map_ = emissive_map;
    }

    void DrawImgui();

private:
    static const size_t downsampled_count = 6;

    struct BloomConstants
    {
        float bloom_extraction_threshold{ 1.5f };
        float bloom_intensity{ 0.25f };
        float bloom_soft_knee{ 0.5f };
        float bloom_radius{ 1.5f };
    };

    struct BloomComputeConstants
    {
        float bloom_extraction_threshold;
        float bloom_intensity;
        float bloom_soft_knee;
        float bloom_radius;

        uint32_t input_width;
        uint32_t input_height;
        uint32_t output_width;
        uint32_t output_height;
    };

    struct BloomTexture
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;

        uint32_t width = 0;
        uint32_t height = 0;
    };

private:
    void CreateBloomTexture(
        ID3D11Device* device,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format,
        BloomTexture& out_texture
    );

    void Dispatch2D(
        ID3D11DeviceContext* context,
        uint32_t width,
        uint32_t height
    );

    void UnbindComputeResources(ID3D11DeviceContext* context);

private:
    // 入力 emissive map
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emissive_map_ = nullptr;

    // Bloom中間テクスチャ
    std::array<BloomTexture, downsampled_count> bloom_mips_;
    std::array<BloomTexture, downsampled_count> bloom_temp_;

    // Compute shaders
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> bloom_extract_cs_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> bloom_downsample_cs_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> bloom_upsample_cs_ = nullptr;

    // Constant buffer
    Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer_ = nullptr;

    BloomConstants bloom_constant_{};

    uint32_t base_width_ = 0;
    uint32_t base_height_ = 0;
};