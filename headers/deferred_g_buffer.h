#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

class DeferredGBuffer
{
public:
    enum class Target : uint8_t
    {
        BaseColor = 0,
        Emissive,
        NormalDepth,
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

    void Clear(ID3D11DeviceContext* ctx);
    void Activate(ID3D11DeviceContext* ctx);
    void Deactivate(ID3D11DeviceContext* ctx);

    ID3D11ShaderResourceView* GetSRV(Target t) const
    {
        return shader_resource_views_[static_cast<uint8_t>(t)].Get();
    }

    ID3D11ShaderResourceView* GetDepthSRV() const
    {
        return depth_srv_.Get();
    }

private:
    static constexpr uint8_t kRTCount = static_cast<uint8_t>(Target::Count);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> render_targets_[kRTCount];
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtvs_[kRTCount];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_views_[kRTCount];

    Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_texture_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth_srv_;

    // cache (FrameBuffer ‚Æ“¯‚¶Žv‘z)
    UINT viewport_count_{};
    D3D11_VIEWPORT cached_viewports_[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> cached_rtvs_[kRTCount];
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> cached_dsv_;

    D3D11_VIEWPORT viewport_{};
};
