#ifndef PART2_FRAMEBUFFER_H
#define PART2_FRAMEBUFFER_H

#include<d3d11.h>
#include<cstdint>
#include<wrl.h>
#include<DirectXMath.h>

//オフスクリーンレンダリング用クラス
class FrameBuffer
{
public:
    enum class usage
    {
        color = 0x01,
        depth = 0x02,
        stencil = 0x04,
        color_depth_stencil = color | depth | stencil,
        color_depth = color | depth,
        depth_stencil = depth | stencil,
    };

    FrameBuffer(ID3D11Device* device, uint32_t width, uint32_t height, usage flags = usage::color_depth_stencil, bool enable_msaa = false, int subsamples = 1, bool generate_mips = false);
    ~FrameBuffer() = default;

    //画面のクリア
    void Clear(ID3D11DeviceContext* immediate_context, usage flags = usage::color_depth_stencil, DirectX::XMFLOAT4 color = { 0.0f, 0.0f, 0.0f, 0.f }, float depth = 1, uint8_t stencil = 0);
    //レンダリング開始
    void Activate(ID3D11DeviceContext* immediate_context, usage flags = usage::color_depth_stencil);
    //レンダリング終了
    void Deactivate(ID3D11DeviceContext* immediate_context);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetShaderResourceView(int num) { return shader_resource_views_[num]; }

private:
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_stencil_view_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_views_[2];
    D3D11_VIEWPORT viewport_;
    UINT viewport_count_{ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };
    D3D11_VIEWPORT cached_viewports_[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> cached_render_target_view_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> cached_depth_stencil_view_;
};
#endif // !PART2_FRAMEBUFFER_H
