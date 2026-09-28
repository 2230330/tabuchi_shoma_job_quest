#ifndef PART2_GRAPHICS_H
#define PART2_GRAPHICS_H

#include<d3d11.h>
#include<wrl.h>

//2026/09/06
//DirectX12の追加
#include<d3d12.h>
#include<dxgi1_6.h>

#pragma comment(lib ,"d3d12.lib")
#pragma comment(lib ,"dxgi.lib")

#include<memory>
#include<vector>

//前方宣言
class RenderState;

//画像描画関係を管理しているクラスです。
class Graphics
{
private:
    Graphics() = default;
    ~Graphics();

public:
    static Graphics& Instance();

    void Shutdown()
    {
        if (immediate_context_) {
            immediate_context_->ClearState();
            immediate_context_->Flush();
        }

        swap_chain_.Reset();
        render_target_view_.Reset();
        depth_stencil_view_.Reset();
        immediate_context_.Reset();
        device_.Reset();

    }

    //初期化
    void Initialize(HWND hwnd);

    void Finalize();

    //画面のクリア
    void ViewClear(float r, float g, float b, float a);

    //レンダーターゲット設定
    void SetRenderTargets();

    //画面表示
    void Present(UINT syncInterval);

    //ウィンドウハンドル取得
    HWND GetWindowHandle() { return this->hwnd_11_; }
    //デバイス取得
    ID3D11Device* GetDevice() { return this->device_.Get(); }
    //デバイスコンテキスト取得
    ID3D11DeviceContext* GetDeviceContext() { return this->immediate_context_.Get(); }
    //スクリーン幅取得
    float GetScreenWidth()const { return this->screen_width_; }
    //スクリーン高さ取得
    float GetScreenHeight()const { return this->screen_height_; }
    //レンダーステート取得
    RenderState* GetRenderState();

    //コンスタントバッファビュー設定
    void SetConstantBuffer(int start_slot, int num, ID3D11Buffer* const* constant_buffers);
    //シェーダーリソースビュー設定
    void SetShaderResource(int start_slot, int num, ID3D11ShaderResourceView* const* shader_resources);
    //サンプラーステート設定
    void SetSampler(int start_slot, int num, ID3D11SamplerState* const* sampler_state);
    //シェーダー解錠
    void ClearShaderSlots();
    //コンスタントバッファビューの解除
    void ClearConstantBuffers(int start_slot = 0, int num = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
    //シェーダーリソースビューの解除
    void ClearShaderResourceViews(int start_slot=0, int num = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
    //サンプラーステートの解除
    void ClearSampler(int start_slot = 0, int num = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT);

    //カメラ用ホイール情報
    void SetWheel(float wheel) { this->wheel_ = wheel; }
    float GetWheel()const { return this->wheel_; }

private:
	void InitializeDirectX11();
	void InitializeDirectX12();

    //メンバ変数
    HWND                                            hwnd_11_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Device>            device_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>     immediate_context_=nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain>          swap_chain_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  render_target_view_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  depth_stencil_view_=nullptr;
    D3D11_VIEWPORT                                  viewport_{};

    //2026_09_28
    //DirectX12の追加、hwndの個別化
	HWND hwnd_12_ = nullptr;

    //2026/09/06
    //DirectX12の追加、実際の業界の環境に近づけます。
    Microsoft::WRL::ComPtr<ID3D12Device> dx12_device_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGIFactory6> idxgi_factory_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> dx12_swap_chain_ = nullptr;
    //コマンドリストとコマンドアロケーター
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmd_allocater_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmd_list_ = nullptr;
    //コマンドキュー
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>cmd_queue_ = nullptr;
    //2026/09/28
    //アダプターの独立とswapchainの独立
    Microsoft::WRL::ComPtr<IDXGIAdapter1>dxgi_adapter_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>rtv_heap_ = nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>dx12_back_buffers_;
    UINT rtv_descriptor_size_ = 0;
    UINT back_buffer_index_ = 0;
	//2026/09/28
	//フェンスの追加
    Microsoft::WRL::ComPtr<ID3D12Fence>dx12_fence_ = nullptr;
	UINT64 dx12_fence_value_ = 0;
	HANDLE dx12_fence_event_ = nullptr;

    float screen_width_  = 0;
    float screen_height_ = 0;
    float wheel_{ 0 };

    std::unique_ptr<RenderState>                    render_state_=nullptr;

};
#endif // !PART2_GRAPHICS_H_
