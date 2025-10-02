#ifndef PART2_GRAPHICS_H
#define PART2_GRAPHICS_H

#include<d3d11.h>
#include<wrl.h>

#include<memory>

#include"../headers/render_state.h"

class Graphics
{
private:
    Graphics() = default;
    ~Graphics() = default;

public:
    static Graphics& Instance() {
        static Graphics instance_;
        return instance_;
    }

    //初期化
    void Initialize(HWND hwnd);

    //画面のクリア
    void ViewClear(float r, float g, float b, float a);

    //レンダーターゲット設定
    void SetRenderTargets();

    //画面表示
    void Present(UINT syncInterval);

    //ウィンドウハンドル取得
    HWND GetWindowHandle() { return this->hwnd_; }
    //デバイス取得
    ID3D11Device* GetDevice() { return this->device_.Get(); }
    //デバイスコンテキスト取得
    ID3D11DeviceContext* GetDeviceContext() { return this->immediate_context_.Get(); }
    //スクリーン幅取得
    float GetScreenWidth()const { return this->screen_width_; }
    //スクリーン高さ取得
    float GetScreenHeight()const { return this->screen_height_; }
    //レンダーステート取得
    RenderState* GetRenderState() { return this->render_state_.get(); }

private:
    //メンバ変数
    HWND                                            hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Device>            device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>     immediate_context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain>          swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  render_target_view_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  depth_stencil_view_;
    D3D11_VIEWPORT                                  viewport_;

    float screen_width_  = 0;
    float screen_height_ = 0;

    std::unique_ptr<RenderState>                    render_state_;
};
#endif // !PART2_GRAPHICS_H_
