
#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <vector>

//前方宣言
class FrameBuffer;

//IBLマネージャ
//生成　diffuse,specular,lut,sky_cube
//オブジェクト描画前に、IBLリソースをバインドする関数
class IBLManager {
public:
    void Initialize(ID3D11Device* dev);

    void SetDirty() { dirty_ = true; }
    bool IsDirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }


    // 背景ソース
    void BuildSkyCubeFromEnvSource();

    // Diffuse
    void UpdateDiffuseSH();

    // Specular Prefilter（GGX）更新
    void UpdateSpecularPrefilter();

    // オブジェクト描画前に IBL リソースをバインド
    void BindForObjectPass(ID3D11DeviceContext* ctx);

    // 背景描画用：SkyRenderSystem へ渡す SkyCube の SRV
    ID3D11ShaderResourceView* GetSkyCubeSRV() const { return sky_cube_srv_.Get(); }
    
    //背景に雲があるのかどうかを知るための関数
    void SetCloudFlag(bool cloud_flag) { cloud_flag_ = cloud_flag; }
    //空があるかどうかを知るための関数
    void SetSkyFlag(bool sky_flag) { sky_flag_ = sky_flag; }

private:
    // パラメータ
    static constexpr UINT kCubeFaces = 6;
    static constexpr UINT kPrefilterSize = 256; // Specular キューブ
    static constexpr UINT kBrdfLutSize = 1024; // BRDF LUT
    static constexpr UINT kSkyCubeSize = 256; // SkyCube 解像度（背景表示/IBL入力に十分）

    // ヘルパ
    static UINT  CalcMipCount(UINT size);


    //DDSにセーブ
    void SaveTextureToDDS(ID3D11Texture2D* tex, const wchar_t* filepath,bool force_srgb=false);

    //SH9用バッファ生成
    //void CreateSHResources(ID3D11Device* device);

    //スペキュラー情報の更新
    void BeginPmpemBaking()
    {
        if (pmrem_baking_)return;//既にフラグが立っている時は何もしない

        pmrem_baking_ = true;
        prefilter_next_face_ = 0; prefilter_next_mip_ = 0;
        pmrem_mip_count_ = CalcMipCount(kPrefilterSize);
    }
private:
    // フラグ
    bool dirty_ = true;  // Specular/SH の再生成が必要なとき true
    bool want_save_dds_ = false;//DDS保存フラグ
    bool sky_flag_ = false;//空があるかどうか
    bool cloud_flag_ = false;//雲があるかどうか

    // デバイス/コンテキスト
    Microsoft::WRL::ComPtr<ID3D11Device>        dev_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx_=nullptr;

    // シェーダ
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> cs_brdf_lut_=nullptr;        // BRDF LUT 生成
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  ibl_screen_vs_=nullptr;      // 全面描画
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   ps_prefilter_=nullptr;       // Prefilter
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   ps_diffuse_=nullptr;       // Diffuse

    // サンプラ
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  samp_linear_clamp_=nullptr;

    // SkyCube（未フィルタ、背景描画/IBL入力で使用）
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           sky_cube_tex_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  sky_cube_srv_=nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> sky_cube_rtvs_; 
    struct SkyCubeCB {
        UINT  faceIndex;
        float _pad[3]; // 16byte アライメント
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb_sky_cube_=nullptr; // b0: face

    // Specular Prefilter 出力（キューブ）
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          prefilter_tex_[2];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> prefilter_srv_[2]; // 表示用
    std::vector<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> prefilter_rtvs_[2]; // face×mip
    UINT pmrem_write_index_ = 0;
    UINT pmrem_read_index_ = 1;

    bool pmrem_baking_ = false;
    UINT pmrem_mip_count_ = 0;

    // Diffuse  出力（キューブ）
    //ping/pongを採用、ディフューズの自然な色の変化を目的に導入
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          diffuse_tex_[2];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> diffuse_srv_[2]; // t1
    std::vector<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> diffuse_rtvs_[2]; // face×mip
    UINT diffuse_write_index_ = 0;//書き込み元
    UINT diffuse_read_index_ = 1;//読み込み元

    // BRDF LUT
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           brdf_lut_tex_=nullptr; // RG16F
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> brdf_lut_uav_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  brdf_lut_srv_=nullptr; // t2

    // 定数バッファ
    // PS用CB（roughness / face index）
    struct PrefilterCB {
        UINT  faceIndex;
        float roughness;
        float mip_count;
        float env_resolution;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb_prefilter_; // b0: roughness/face
    struct DiffuseCB {
        UINT  faceIndex;
        UINT frameIndex;
        float alpha{0.05};
        float mip_lod{ 1.f };
        };
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb_diffuse_; // b0: roughness/face
    UINT frame_index_ = 0;

private:
    // 公開 SRV / CB（オブジェクトパスで使用）
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_pref_env_=nullptr; // t1: Specular Prefiltered Env
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_brdf_lut_=nullptr; // t2: BRDF LUT

    Microsoft::WRL::ComPtr<ID3D11PixelShader>sky_cube_ps_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>cloud_cube_ps_=nullptr;

private:
    // 分割更新インデックス（Specular Prefilter 用）
    UINT prefilter_next_face_ = 0;
    UINT prefilter_next_mip_ = 0;

    //分割更新インデックス（SkyCube用）
    UINT sky_cube_next_face_ = 0;

private:
        //雲用ノイズテクスチャ
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>low_freq_perlin_worley_srv_ = nullptr;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>mid_freq_worley_srv_ = nullptr;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>high_freq_worley_srv_ = nullptr;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>weather_map_srv_ = nullptr;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>curl_noise_srv_ = nullptr;

        //雲ボックス
        Microsoft::WRL::ComPtr<ID3D11Texture2D>           cloud_cube_tex_=nullptr;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  cloud_cube_srv_=nullptr;
        std::vector<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> cloud_cube_rtvs_; // face×mip

};
