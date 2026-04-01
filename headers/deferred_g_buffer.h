#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include<memory>

#include"render_state.h"

//ディファードレンダリング描画用フレームバッファ
class DeferredGBuffer
{
public:
    enum Target : uint8_t
    {
        BaseColor = 0,
        Emissive,
        Normal,
        Parameter,
        Depth,
        Velocity,
        Count
    };

public:
    DeferredGBuffer(
        ID3D11Device* device,
        uint32_t width,
        uint32_t height,
        bool enable_msaa = false,
        int subsamples = 1
    );

    ~DeferredGBuffer() = default;

    void Clear(ID3D11DeviceContext* ctx);
    void Activate(ID3D11DeviceContext* ctx);
    void Deactivate(ID3D11DeviceContext* ctx);

    ID3D11ShaderResourceView* GetSRV(int t) const
    {
        return shader_resource_views_[static_cast<uint8_t>(t)].Get();
    }

    ID3D11ShaderResourceView* GetDepthSRV() const
    {
        return depth_srv_.Get();
    }

    //ディファードレンダリングの基本データ
    //ライティング前に呼び出す関数
    void SetDeferredCommonResource();

private:
    static constexpr uint8_t kRTCount = static_cast<uint8_t>(Target::Count);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> render_targets_[kRTCount];
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtvs_[kRTCount];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_views_[kRTCount];
    //αブレンドを行うと変な具合になるので、こちら側で設定します
    Microsoft::WRL::ComPtr<ID3D11BlendState>blend_state_;

    //アクティブ前に入っていたRTVの数
    UINT cached_num_rtvs_ = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_texture_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth_srv_;

    // cache (FrameBuffer と同じ思想)
    UINT viewport_count_{};
    D3D11_VIEWPORT cached_viewports_[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> cached_rtvs_[kRTCount];
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> cached_dsv_;

    D3D11_VIEWPORT viewport_{};
    std::unique_ptr<RenderState>render_state_ = nullptr;
};
