
#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <vector>

#include "../framebuffer.h"

// IBL（Image Based Lighting）＋ SkyCube（未フィルタ）の軽量マネージャ
// - 起動時: BRDF LUT を CS で生成
// - 背景ソース更新時: FrameBuffer の SRV を受け取り → LatLong->Cube 変換で SkyCube を再生成
// - Diffuse: SH(9係数) は dirty 時だけ再計算（まずは簡易でも可）
// - Specular: PSでキューブmipへ分割更新（dirty 時だけ）
// - Object描画前: PrefEnv+BRDF LUT+SH をバインド
class IBLManager {
public:
    void Initialize(ID3D11Device* dev);

    void SetDirty() { dirty_ = true; }
    bool IsDirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }

    // 背景ソース（back_fb の SRV）を受け取り、内部の env_source_srv_ を更新
    void UpdateEnvironmentCapture(const FrameBuffer& back_fb);

    // 背景ソース（LatLong 2D）→ SkyCube（TextureCube）へ変換（CS）
    void BuildSkyCubeFromEnvSource();

    // Diffuse（SH 9係数）更新（軽量版）
    void UpdateDiffuseSH();

    // Specular Prefilter（GGX）更新：分割更新（face×mip）に対応
    void UpdateSpecularPrefilter();

    // オブジェクト描画前に IBL リソースをバインド
    void BindForObjectPass(ID3D11DeviceContext* ctx);

    // 背景描画用：SkyRenderSystem へ渡す SkyCube の SRV
    ID3D11ShaderResourceView* GetSkyCubeSRV() const { return sky_cube_srv_.Get(); }

private:
    // パラメータ
    static constexpr UINT kCubeFaces = 6;
    static constexpr UINT kPrefilterSize = 256; // Specular キューブ
    static constexpr UINT kBrdfLutSize = 256; // BRDF LUT
    static constexpr UINT kSkyCubeSize = 512; // SkyCube 解像度（背景表示/IBL入力に十分）

    // ヘルパ
    static UINT  CalcMipCount(UINT size);
    static HRESULT LoadCSO(const wchar_t* path, Microsoft::WRL::ComPtr<ID3DBlob>& blob);

    //DDSにセーブ
    void SaveTextureToDDS(ID3D11Texture2D* tex, const wchar_t* filepath,bool force_srgb=false);

private:
    // フラグ
    bool dirty_ = true;  // Specular/SH の再生成が必要なとき true
    bool want_save_dds_ = false;//DDS保存フラグ

    // デバイス/コンテキスト
    Microsoft::WRL::ComPtr<ID3D11Device>        dev_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx_;

    // シェーダ
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> cs_brdf_lut_;        // BRDF LUT 生成
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> cs_latlong_to_cube_; // LatLong -> Cube 変換
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vs_fullscreen_;      // 全面描画
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   ps_prefilter_;       // Prefilter

    // サンプラ
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  samp_linear_clamp_;

    // 背景ソース（LatLong/Equirect などの 2D SRV）
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> env_source_srv_;

    // SkyCube（未フィルタ、背景描画/IBL入力で使用）
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           sky_cube_tex_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  sky_cube_srv_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> sky_cube_uav_; // CS で書く用

    // Specular Prefilter 出力（キューブ）
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          prefilter_tex_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> prefilter_srv_; // t1
    std::vector<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> prefilter_rtvs_; // face×mip

    // BRDF LUT
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           brdf_lut_tex_; // RG16F
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> brdf_lut_uav_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  brdf_lut_srv_; // t2

    // 定数バッファ
    // PS用CB（roughness / face index）
    struct PrefilterCB {
        UINT  faceIndex;
        float roughness;
        float mip_count;
        float _pad; // 16byte アライメント
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb_prefilter_; // b0: roughness/face

private:
    // 公開 SRV / CB（オブジェクトパスで使用）
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_pref_env_; // t1: Specular Prefiltered Env
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_brdf_lut_; // t2: BRDF LUT
    // SH 係数（float3 × 9）
    struct SH9Constants {
        DirectX::XMFLOAT3 c[9];
        float _pad; // 16byte アライメント
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer>             cb_sh_;        // b2: SH(9係数)

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>cube_rtv_all_;//全スライス一括
    Microsoft::WRL::ComPtr<ID3D11GeometryShader>latlong_to_cube_gs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>latlong_to_cube_ps_;

private:
    // 分割更新インデックス（Specular Prefilter 用）
    UINT prefilter_next_face_ = 0;
    UINT prefilter_next_mip_ = 0;
};
