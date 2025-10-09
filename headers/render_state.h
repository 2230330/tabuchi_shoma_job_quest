#ifndef PART2_RENDER_STATE_H
#define PART2_RENDER_STATE_H

#include<d3d11.h>
#include<wrl.h>

//サンプラーステート
enum class SamplerState
{
    point_wrap,
    point_clamp,
    linear_wrap,
    linear_clamp,
    anisotropic,

    enum_count
};
//デプスステート
enum class DepthState
{
    test_and_write,
    test_only,
    write_only,
    no_test_no_write,

    enum_count
};
//ブレンドステート
enum class BlendState
{
    opaque,
    transparency,
    additive,
    subtraction,
    multiply,

    enum_count
};
//ラスタライザステート
enum class RasterizerState
{
    solid_cull_none,
    solid_cull_back,
    wire_cull_none,
    wire_cull_back,

    enum_count
};

//レンダーステート
class RenderState
{
public:
    RenderState(ID3D11Device* device);
    ~RenderState() = default;

    //サンプラーステート取得
    ID3D11SamplerState* GetSamplerState(SamplerState state)const
    {
        return this->sampler_state_[static_cast<int>(state)].Get();
    }
    //デプスステート取得
    ID3D11DepthStencilState* GetDepthStencilState(DepthState state)const
    {
        return this->depth_stencil_state_[static_cast<int>(state)].Get();
    }
    //ブレンドステート取得
    ID3D11BlendState* GetBlendState(BlendState state)const
    {
        return this->blend_state_[static_cast<int>(state)].Get();
    }
    //ラスタライザステート取得
    ID3D11RasterizerState* GetRasterizerState(RasterizerState state)const
    {
        return this->rasterizer_state_[static_cast<int>(state)].Get();
    }

private:
    Microsoft::WRL::ComPtr<ID3D11SamplerState>       sampler_state_[static_cast<int>(SamplerState::enum_count)];
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>       depth_stencil_state_[static_cast<int>(DepthState::enum_count)];
    Microsoft::WRL::ComPtr<ID3D11BlendState>       blend_state_[static_cast<int>(BlendState::enum_count)];
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>       rasterizer_state_[static_cast<int>(RasterizerState::enum_count)];
};

#endif //!PART2_RENDER_STATE